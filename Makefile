# October Production Co. - Multi-Plugin Monorepo Build System
#
# Adding a new plugin:
#   1. Add directory under plugins/<name>/
#   2. Add to JUCE_PLUGINS (and VCV_PLUGINS if applicable) below
#   3. Add configure/test presets to CMakePresets.json
#   4. Add CMakeLists.txt entries in root CMakeLists.txt

.DEFAULT_GOAL := help

# ── Plugin registry ─────────────────────────────────────────────
JUCE_PLUGINS := octobir octobass
VCV_PLUGINS  := octobir
ALL_PLUGINS  := $(sort $(JUCE_PLUGINS) $(VCV_PLUGINS))

# ── Platform detection ──────────────────────────────────────────
UNAME_S := $(shell uname -s)
NPROC   := $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
# Test binaries are built with ASan (-fsanitize=address) via CMake presets.
# On Linux, LSan (leak detection) is supported on top of ASan.
# On macOS, Apple Clang does not support LSan — ASan alone handles memory errors.
# Note: macOS `leaks` tool is incompatible with ASan's malloc replacement.
ifeq ($(UNAME_S),Darwin)
    TEST_RUNNER :=
    HOST_ARCH       := $(shell uname -m)
    MACOS_ARCH_FLAG := -DCMAKE_OSX_ARCHITECTURES=$(HOST_ARCH) -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
else
    TEST_RUNNER := ASAN_OPTIONS=detect_leaks=1
    MACOS_ARCH_FLAG :=
endif

ALL_SOURCES  := $(shell find libs plugins -name "*.cpp" -o -name "*.hpp" -o -name "*.h")
CORE_SOURCES := $(shell find libs/octobir-core/src -name "*.cpp")
VCV_SOURCES  := $(shell find plugins/octobir/vcv-rack/src -name "*.cpp" 2>/dev/null)

# ── Convenience aggregate targets ──────────────────────────────
.PHONY: octobir octobass all test core header-opc header-octobir header-octobass help clean tidy format license
.PHONY: octobir-juce octobir-vcv octobass-juce
.PHONY: test-octobir test-octobir-core test-octobir-juce test-octobir-vcv
.PHONY: test-octobass test-octobass-core test-octobass-juce

header-opc:
	@./scripts/show-header.sh opc

header-octobir:
	@./scripts/show-header.sh octobir

header-octobass:
	@./scripts/show-header.sh octobass

help: header-opc
	@echo "Usage: make <target>"
	@echo ""
	@echo "Build and install:"
	@echo "  make octobir          - Build all OctobIR formats (JUCE + VCV)"
	@echo "  make octobir-juce     - Build and install OctobIR JUCE plugin"
	@echo "  make octobir-vcv      - Build and install OctobIR VCV plugin"
	@echo "  make octobass         - Build all OctoBASS formats (JUCE)"
	@echo "  make octobass-juce    - Build and install OctoBASS JUCE plugin"
	@echo "  make core             - Build core libraries only (debug)"
	@echo ""
	@echo "Testing (with ASan + leak detection):"
	@echo "  make test             - Run all tests"
	@echo "  make test-octobir     - Run all OctobIR tests"
	@echo "  make test-octobir-core  - Run octobir-core unit tests"
	@echo "  make test-octobir-juce  - Run OctobIR JUCE plugin tests"
	@echo "  make test-octobir-vcv   - Run OctobIR VCV plugin tests"
	@echo "  make test-octobass        - Run all OctoBASS tests"
	@echo "  make test-octobass-core - Run octobass-core unit tests"
	@echo "  make test-octobass-juce - Run OctoBASS JUCE plugin tests"
	@echo ""
	@echo "Code quality:"
	@echo "  make tidy             - Run formatting, static analysis, and license checks"
	@echo "  make format           - Auto-format all code with clang-format"
	@echo "  make license          - Check REUSE license compliance"
	@echo ""
	@echo "Other:"
	@echo "  make clean            - Remove all build artifacts"
	@echo "  make help             - Show this help message"
	@echo ""
	@echo "Note: GitHub releases provide pre-built installers for end users"

# ── OctobIR aggregate ──────────────────────────────────────────
octobir: octobir-juce octobir-vcv

octobir-juce: header-octobir
	@echo "Building and installing OctobIR JUCE plugin (Release)..."
ifeq ($(UNAME_S),Darwin)
	@echo "  Target architecture: $(HOST_ARCH)"
endif
	@rm -rf build/release-juce
	@cmake -B build/release-juce \
		-DCMAKE_BUILD_TYPE=Release \
		-DBUILD_OCTOBIR_JUCE=ON \
		-DBUILD_OCTOBIR_VCV=OFF \
		-DBUILD_OCTOBASS=OFF \
		-DBUILD_OCTOBASS_JUCE=OFF \
		$(MACOS_ARCH_FLAG)
	@cmake --build build/release-juce --target OctobIR_All --config Release -j$(NPROC)
	@echo ""
	@echo "Installing plugins to system directories..."
	@if [ -e ~/Library/Audio/Plug-Ins/VST3/OctobIR.vst3 ] && [ ! -w ~/Library/Audio/Plug-Ins/VST3/OctobIR.vst3 ]; then \
		echo ""; \
		echo "Error: Existing VST3 plugin is not writable (may be owned by root)."; \
		echo "Please remove it first:"; \
		echo "  sudo rm -rf ~/Library/Audio/Plug-Ins/VST3/OctobIR.vst3"; \
		echo "  sudo rm -rf ~/Library/Audio/Plug-Ins/Components/OctobIR.component"; \
		echo "Then run: make octobir-juce"; \
		echo ""; \
		exit 1; \
	fi
	@cp -rf build/release-juce/plugins/octobir/juce/OctobIR_artefacts/Release/VST3/OctobIR.vst3 \
		~/Library/Audio/Plug-Ins/VST3/
	@echo "  VST3 installed"
	@cp -rf build/release-juce/plugins/octobir/juce/OctobIR_artefacts/Release/AU/OctobIR.component \
		~/Library/Audio/Plug-Ins/Components/
	@echo "  AU installed"
	@echo ""
	@echo "OctobIR JUCE plugin installed:"
	@echo "  VST3: ~/Library/Audio/Plug-Ins/VST3/OctobIR.vst3"
	@echo "  AU:   ~/Library/Audio/Plug-Ins/Components/OctobIR.component"

octobir-vcv: header-octobir
	@cmake --preset dev-octobir-vcv
	@cmake --build build/dev-octobir-vcv --target vcv-plugin-clean || true
	@cmake --build build/dev-octobir-vcv --target vcv-plugin -j$(NPROC)
	@cmake --build build/dev-octobir-vcv --target vcv-plugin-install
	@echo "OctobIR VCV plugin cleaned, built, and installed"

# ── OctoBASS aggregate ─────────────────────────────────────────
octobass: octobass-juce

octobass-juce: header-octobass
	@echo "Building and installing OctoBASS JUCE plugin (Release)..."
ifeq ($(UNAME_S),Darwin)
	@echo "  Target architecture: $(HOST_ARCH)"
endif
	@rm -rf build/release-octobass-juce
	@cmake -B build/release-octobass-juce \
		-DCMAKE_BUILD_TYPE=Release \
		-DBUILD_OCTOBIR=OFF \
		-DBUILD_OCTOBIR_JUCE=OFF \
		-DBUILD_OCTOBIR_VCV=OFF \
		-DBUILD_OCTOBASS_JUCE=ON \
		$(MACOS_ARCH_FLAG)
	@cmake --build build/release-octobass-juce --target OctoBASS_All --config Release -j$(NPROC)
	@echo ""
	@echo "Installing plugins to system directories..."
	@if [ -e ~/Library/Audio/Plug-Ins/VST3/OctoBASS.vst3 ] && [ ! -w ~/Library/Audio/Plug-Ins/VST3/OctoBASS.vst3 ]; then \
		echo ""; \
		echo "Error: Existing VST3 plugin is not writable (may be owned by root)."; \
		echo "Please remove it first:"; \
		echo "  sudo rm -rf ~/Library/Audio/Plug-Ins/VST3/OctoBASS.vst3"; \
		echo "  sudo rm -rf ~/Library/Audio/Plug-Ins/Components/OctoBASS.component"; \
		echo "Then run: make octobass-juce"; \
		echo ""; \
		exit 1; \
	fi
	@cp -rf build/release-octobass-juce/plugins/octobass/juce/OctoBASS_artefacts/Release/VST3/OctoBASS.vst3 \
		~/Library/Audio/Plug-Ins/VST3/
	@echo "  VST3 installed"
	@cp -rf build/release-octobass-juce/plugins/octobass/juce/OctoBASS_artefacts/Release/AU/OctoBASS.component \
		~/Library/Audio/Plug-Ins/Components/
	@echo "  AU installed"
	@echo ""
	@echo "OctoBASS JUCE plugin installed:"
	@echo "  VST3: ~/Library/Audio/Plug-Ins/VST3/OctoBASS.vst3"
	@echo "  AU:   ~/Library/Audio/Plug-Ins/Components/OctoBASS.component"

# ── Core libraries ─────────────────────────────────────────────
core: header-opc
	@cmake --preset dev
	@cmake --build build/dev --target octobir-core octobass-core -j$(NPROC)
	@echo "Core libraries built"

# ── Test targets ───────────────────────────────────────────────
test: test-octobir test-octobass

test-octobir: test-octobir-core test-octobir-juce test-octobir-vcv

test-octobir-core:
	@rm -rf build/test-octobir-core
	@cmake --preset test-octobir-core
	@cmake --build build/test-octobir-core --target octobir-core-tests -j$(NPROC)
	@echo "Running octobir-core tests..."
	@$(TEST_RUNNER) ./build/test-octobir-core/libs/octobir-core/tests/octobir-core-tests

test-octobir-juce:
	@rm -rf build/test-octobir-juce
	@cmake --preset test-octobir-juce
	@cmake --build build/test-octobir-juce --target octobir-plugin-tests -j$(NPROC)
	@echo "Running OctobIR JUCE plugin tests..."
	@$(TEST_RUNNER) ./build/test-octobir-juce/plugins/octobir/juce/tests/octobir-plugin-tests

test-octobir-vcv:
	@rm -rf build/test-octobir-vcv
	@cmake --preset test-octobir-vcv
	@cmake --build build/test-octobir-vcv --target octobir-vcv-tests -j$(NPROC)
	@echo "Running OctobIR VCV plugin tests..."
	@$(TEST_RUNNER) ./build/test-octobir-vcv/plugins/octobir/vcv-rack/tests/octobir-vcv-tests

test-octobass: test-octobass-core test-octobass-juce

test-octobass-core:
	@rm -rf build/test-octobass-core
	@cmake --preset test-octobass-core
	@cmake --build build/test-octobass-core --target octobass-core-tests -j$(NPROC)
	@echo "Running octobass-core tests..."
	@$(TEST_RUNNER) ./build/test-octobass-core/libs/octobass-core/tests/octobass-core-tests

test-octobass-juce:
	@rm -rf build/test-octobass-juce
	@cmake --preset test-octobass-juce
	@cmake --build build/test-octobass-juce --target octobass-plugin-tests -j$(NPROC)
	@echo "Running OctoBASS JUCE plugin tests..."
	@$(TEST_RUNNER) ./build/test-octobass-juce/plugins/octobass/juce/tests/octobass-plugin-tests

# ── Clean ──────────────────────────────────────────────────────
clean:
	@rm -rf build
	@echo "All build artifacts cleaned"

# ── Code quality ───────────────────────────────────────────────
tidy:
	@echo "Checking code formatting..."
	@if ! command -v clang-format &> /dev/null; then \
		echo "Error: clang-format not installed. Run ./scripts/setup-dev.sh"; \
		exit 1; \
	fi
	@echo $(ALL_SOURCES) | xargs clang-format --dry-run --Werror --style=file || \
		(echo "Error: Code is not properly formatted. Run 'make format' to fix." && exit 1)
	@echo "Code formatting verified"
	@echo ""
	@echo "Running static analysis..."
	@if ! command -v clang-tidy &> /dev/null; then \
		echo "Error: clang-tidy not installed. Run ./scripts/setup-dev.sh"; \
		exit 1; \
	fi
	@echo $(CORE_SOURCES) | xargs -I {} clang-tidy {} -- \
		-I./libs/octobir-core/include \
		-I./third_party/WDL/WDL \
		-I./third_party \
		-I./third_party/pffft/include/pffft \
		-std=c++11 || \
		(echo "Error: Static analysis found issues" && exit 1)
	@if [ -n "$$RACK_DIR" ] && [ -d "$$RACK_DIR" ]; then \
		echo $(VCV_SOURCES) | xargs -I {} clang-tidy {} -- \
			-I./libs/octobir-core/include \
			-I./third_party/WDL/WDL \
			-I./third_party \
			-I$$RACK_DIR/include \
			-I$$RACK_DIR/dep/include \
			-std=c++11 || \
			(echo "Error: Static analysis found issues in VCV plugin" && exit 1); \
	else \
		echo "  Skipping VCV plugin analysis (RACK_DIR not set or not found)"; \
	fi
	@echo "Static analysis passed"
	@echo ""
	@$(MAKE) --no-print-directory license
	@echo ""
	@echo "All code quality checks passed"

license:
	@echo "Checking license compliance..."
	@if ! command -v reuse &> /dev/null; then \
		echo "Error: reuse not installed. Run ./scripts/setup-dev.sh"; \
		exit 1; \
	fi
	@reuse lint
	@echo "License compliance verified"

format:
	@echo "Formatting code..."
	@if ! command -v clang-format &> /dev/null; then \
		echo "Error: clang-format not installed. Run ./scripts/setup-dev.sh"; \
		exit 1; \
	fi
	@echo $(ALL_SOURCES) | xargs clang-format -i --style=file
	@echo "Code formatted"
