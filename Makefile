# OctobIR Monorepo Build System
#
# Plugin convention:
#   1. Add plugin directory under plugins/<name>/
#   2. Add configure preset (dev-<name>) and test preset (test-<name>) to CMakePresets.json
#   3. Add dev/test/install targets below following the existing pattern
#   4. VCV-style plugins using external Makefiles: wrap with CMake custom targets (see plugins/vcv-rack/)
#   5. JUCE-style plugins: use juce_add_plugin() in CMakeLists.txt (see plugins/juce-multiformat/)

.PHONY: help vcv juce core test test-juce test-vcv clean tidy format license install-vcv header

.DEFAULT_GOAL := help

UNAME_S := $(shell uname -s)
NPROC := $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
ASAN_FLAGS := -fsanitize=address -fno-omit-frame-pointer
ASAN_LINKER_FLAGS := -fsanitize=address
ifeq ($(UNAME_S),Darwin)
    TEST_RUNNER := MallocStackLogging=1 leaks --atExit --
else
    TEST_RUNNER := ASAN_OPTIONS=detect_leaks=1
endif

ALL_SOURCES := $(shell find libs plugins -name "*.cpp" -o -name "*.hpp" -o -name "*.h")
CORE_SOURCES := $(shell find libs/octobir-core/src -name "*.cpp")
VCV_SOURCES := $(shell find plugins/vcv-rack/src -name "*.cpp")

# Display ASCII art header with colors
header:
	@./scripts/show-header.sh

# Default target - display available make targets
help: header
	@echo "Usage: make <target>"
	@echo ""
	@echo "Build and install:"
	@echo "  make juce        - Build and install JUCE plugin (release)"
	@echo "  make vcv         - Build and install VCV plugin (debug)"
	@echo "  make install-vcv - Build and install VCV plugin (release)"
	@echo "  make core        - Build core library only (debug)"
	@echo ""
	@echo "Testing (with ASan + leak detection):"
	@echo "  make test        - Run octobir-core unit tests"
	@echo "  make test-juce   - Run JUCE plugin unit tests"
	@echo "  make test-vcv    - Run VCV plugin unit tests"
	@echo ""
	@echo "Code quality:"
	@echo "  make tidy        - Run formatting, static analysis, and license checks"
	@echo "  make format      - Auto-format all code with clang-format"
	@echo "  make license     - Check REUSE license compliance"
	@echo ""
	@echo "Other:"
	@echo "  make clean       - Remove all build artifacts"
	@echo "  make help        - Show this help message"
	@echo ""
	@echo "Note: GitHub releases provide pre-built installers for end users"

# JUCE Plugin - Build and install (release)
juce: header
	@echo "Building and installing JUCE plugin (Release)..."
	@rm -rf build/release-juce
	@cmake -B build/release-juce \
		-DCMAKE_BUILD_TYPE=Release \
		-DBUILD_JUCE_PLUGIN=ON \
		-DBUILD_VCV_PLUGIN=OFF \
		-DBUILD_TESTS=OFF
	@cmake --build build/release-juce --target OctobIR_All --config Release -j$(NPROC)
	@echo ""
	@echo "Installing plugins to system directories..."
	@if [ -e ~/Library/Audio/Plug-Ins/VST3/OctobIR.vst3 ] && [ ! -w ~/Library/Audio/Plug-Ins/VST3/OctobIR.vst3 ]; then \
		echo ""; \
		echo "Error: Existing VST3 plugin is not writable (may be owned by root)."; \
		echo "Please remove it first:"; \
		echo "  sudo rm -rf ~/Library/Audio/Plug-Ins/VST3/OctobIR.vst3"; \
		echo "  sudo rm -rf ~/Library/Audio/Plug-Ins/Components/OctobIR.component"; \
		echo "Then run: make juce"; \
		echo ""; \
		exit 1; \
	fi
	@cp -rf build/release-juce/plugins/juce-multiformat/OctobIR_artefacts/Release/VST3/OctobIR.vst3 \
		~/Library/Audio/Plug-Ins/VST3/
	@echo "  VST3 installed"
	@cp -rf build/release-juce/plugins/juce-multiformat/OctobIR_artefacts/Release/AU/OctobIR.component \
		~/Library/Audio/Plug-Ins/Components/
	@echo "  AU installed"
	@echo ""
	@echo "JUCE plugin installed:"
	@echo "  VST3: ~/Library/Audio/Plug-Ins/VST3/OctobIR.vst3"
	@echo "  AU:   ~/Library/Audio/Plug-Ins/Components/OctobIR.component"

# VCV Plugin Development - Build and install (debug)
vcv: header
	@cmake --preset dev-vcv
	@cmake --build build/dev-vcv --target vcv-plugin-clean || true
	@cmake --build build/dev-vcv --target vcv-plugin -j$(NPROC)
	@cmake --build build/dev-vcv --target vcv-plugin-install
	@echo "VCV plugin cleaned, built, and installed"

# Core Library - Build only (debug, incremental)
core: header
	@cmake --preset dev
	@cmake --build build/dev --target octobir-core -j$(NPROC)
	@echo "Core library built"

# Core Library Tests
test:
	@rm -rf build/test
	@cmake --preset test-core
	@cmake --build build/test --target octobir-core-tests -j$(NPROC)
	@echo "Running tests..."
	@$(TEST_RUNNER) ./build/test/libs/octobir-core/tests/octobir-core-tests

# VCV Plugin Tests
test-vcv:
	@rm -rf build/test-vcv
	@cmake --preset test-vcv
	@cmake --build build/test-vcv --target octobir-vcv-tests -j$(NPROC)
	@echo "Running VCV plugin tests..."
	@$(TEST_RUNNER) ./build/test-vcv/plugins/vcv-rack/tests/octobir-vcv-tests

# JUCE Plugin Tests
test-juce:
	@rm -rf build/test-juce
	@cmake --preset test-juce
	@cmake --build build/test-juce --target octobir-plugin-tests -j$(NPROC)
	@echo "Running JUCE plugin tests..."
	@$(TEST_RUNNER) ./build/test-juce/plugins/juce-multiformat/tests/octobir-plugin-tests

# Clean target
clean:
	@rm -rf build
	@echo "All build artifacts cleaned"

# Code quality checks
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

# REUSE license compliance check
license:
	@echo "Checking license compliance..."
	@if ! command -v reuse &> /dev/null; then \
		echo "Error: reuse not installed. Run ./scripts/setup-dev.sh"; \
		exit 1; \
	fi
	@reuse lint
	@echo "License compliance verified"

# Auto-format code
format:
	@echo "Formatting code..."
	@if ! command -v clang-format &> /dev/null; then \
		echo "Error: clang-format not installed. Run ./scripts/setup-dev.sh"; \
		exit 1; \
	fi
	@echo $(ALL_SOURCES) | xargs clang-format -i --style=file
	@echo "Code formatted"

# Install VCV Rack plugin (release build)
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
	@cd plugins/vcv-rack && \
		export RACK_DIR=$${RACK_DIR:-../../../Rack} && \
		make clean && \
		make -j$(NPROC) && \
		make install
	@echo ""
	@echo "VCV plugin installed"
