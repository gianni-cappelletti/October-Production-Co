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
ifeq ($(UNAME_S),Darwin)
    TEST_RUNNER := MallocStackLogging=1 leaks --atExit --
else
    TEST_RUNNER := ASAN_OPTIONS=detect_leaks=1
endif

ALL_SOURCES  := $(shell find libs plugins -name "*.cpp" -o -name "*.hpp" -o -name "*.h")
CORE_SOURCES := $(shell find libs/octobir-core/src -name "*.cpp")
VCV_SOURCES  := $(shell find plugins/octobir/vcv-rack/src -name "*.cpp" 2>/dev/null)

# ── JUCE plugin targets (generated per plugin) ─────────────────
# For each JUCE plugin, generates:
#   <name>-juce        Build and install
#   test-<name>-juce   Run tests with ASan
define JUCE_PLUGIN_TARGETS

.PHONY: $(1)-juce test-$(1)-juce

$(1)-juce: header
	@echo "Building and installing $(1) JUCE plugin (Release)..."
	@rm -rf build/release-$(1)-juce
	@cmake --preset release \
		-DBUILD_$(call to_upper,$(1))_JUCE=ON \
		-DBUILD_$(call to_upper,$(1))_VCV=OFF
	@cmake --build build/release --target $(call juce_target,$(1)) --config Release -j$(NPROC)

test-$(1)-juce:
	@rm -rf build/test-$(1)-juce
	@cmake --preset test-$(1)-juce
	@cmake --build build/test-$(1)-juce --target $(call juce_test_target,$(1)) -j$(NPROC)
	@echo "Running $(1) JUCE plugin tests..."
	@$(TEST_RUNNER) ./build/test-$(1)-juce/plugins/$(1)/juce/tests/$(call juce_test_target,$(1))

endef

# Helper functions
to_upper = $(shell echo $(1) | tr '[:lower:]' '[:upper:]')
juce_target = $(if $(filter octobir,$(1)),OctobIR_All,$(if $(filter octobass,$(1)),OctoBASS_All,$(1)_All))
juce_test_target = $(if $(filter octobir,$(1)),octobir-plugin-tests,$(if $(filter octobass,$(1)),octobass-plugin-tests,$(1)-plugin-tests))

$(foreach p,$(JUCE_PLUGINS),$(eval $(call JUCE_PLUGIN_TARGETS,$(p))))

# ── Convenience aggregate targets ──────────────────────────────
.PHONY: octobir octobass all test core header help clean tidy format license
.PHONY: octobir-vcv test-octobir test-octobir-core test-octobir-vcv test-octobass install-vcv

header:
	@./scripts/show-header.sh

help: header
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
	@echo "  make test-octobass      - Run all OctoBASS tests"
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

octobir-juce: header
	@echo "Building and installing OctobIR JUCE plugin (Release)..."
	@rm -rf build/release-juce
	@cmake -B build/release-juce \
		-DCMAKE_BUILD_TYPE=Release \
		-DBUILD_OCTOBIR_JUCE=ON \
		-DBUILD_OCTOBIR_VCV=OFF \
		-DBUILD_OCTOBASS=OFF \
		-DBUILD_OCTOBASS_JUCE=OFF
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

octobir-vcv: header
	@cmake --preset dev-octobir-vcv
	@cmake --build build/dev-octobir-vcv --target vcv-plugin-clean || true
	@cmake --build build/dev-octobir-vcv --target vcv-plugin -j$(NPROC)
	@cmake --build build/dev-octobir-vcv --target vcv-plugin-install
	@echo "OctobIR VCV plugin cleaned, built, and installed"

# ── OctoBASS aggregate ─────────────────────────────────────────
octobass: octobass-juce

octobass-juce: header
	@echo "Building and installing OctoBASS JUCE plugin (Release)..."
	@rm -rf build/release-octobass-juce
	@cmake -B build/release-octobass-juce \
		-DCMAKE_BUILD_TYPE=Release \
		-DBUILD_OCTOBIR=OFF \
		-DBUILD_OCTOBIR_JUCE=OFF \
		-DBUILD_OCTOBIR_VCV=OFF \
		-DBUILD_OCTOBASS_JUCE=ON
	@cmake --build build/release-octobass-juce --target OctoBASS_All --config Release -j$(NPROC)
	@echo ""
	@echo "OctoBASS JUCE plugin built successfully"

# ── Core libraries ─────────────────────────────────────────────
core: header
	@cmake --preset dev
	@cmake --build build/dev --target octobir-core -j$(NPROC)
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

test-octobass: test-octobass-juce

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

# ── VCV install (release) ──────────────────────────────────────
install-vcv:
	@if [ -z "$$RACK_DIR" ] && [ ! -d "../Rack" ] && [ ! -d "../../Rack" ]; then \
		echo "Error: RACK_DIR not set and VCV Rack SDK not found"; \
		echo ""; \
		echo "Please set RACK_DIR to your VCV Rack SDK location:"; \
		echo "  export RACK_DIR=/path/to/Rack"; \
		echo "  make install-vcv"; \
		echo ""; \
		echo "Or download the SDK to ../Rack or ../../Rack"; \
		exit 1; \
	fi
	@echo "Building and installing VCV plugin (Release)..."
	@./scripts/sync-vcv-version.sh
	@cd plugins/octobir/vcv-rack && \
		export RACK_DIR=$${RACK_DIR:-../../../../Rack} && \
		make clean && \
		make -j$(NPROC) && \
		make install
	@echo ""
	@echo "VCV plugin installed"
