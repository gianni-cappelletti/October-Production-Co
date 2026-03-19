.PHONY: help vcv juce core test clean tidy format install-juce install-vcv header

.DEFAULT_GOAL := help

# Display ASCII art header with colors
header:
	@./scripts/show-header.sh

# Default target - display available make targets
# Run 'make' or 'make help' to see this message
help: header
	@echo "Usage: make <target>"
	@echo ""
	@echo "Development build targets (always clean build):"
	@echo "  make vcv         - Clean, build, and install VCV plugin (debug)"
	@echo "  make juce        - Clean and build JUCE plugin (debug)"
	@echo "  make core        - Clean and build core library only"
	@echo "  make test        - Build and run unit tests"
	@echo ""
	@echo "Install targets (release builds):"
	@echo "  make install-juce - Build and install JUCE plugin (release)"
	@echo "  make install-vcv  - Build and install VCV plugin (release)"
	@echo ""
	@echo "Code quality:"
	@echo "  make tidy        - Run formatting and static analysis checks"
	@echo "  make format      - Auto-format all code with clang-format"
	@echo ""
	@echo "Other:"
	@echo "  make clean       - Remove all build artifacts"
	@echo "  make help        - Show this help message"
	@echo ""
	@echo "Note: GitHub releases provide pre-built installers for end users"

# VCV Plugin Development
vcv: header
	@if cmake --preset dev-vcv; then \
		echo "Using CMake preset: dev-vcv"; \
	else \
		echo "CMake preset not available, using manual configuration"; \
		cmake -B build/dev-vcv -DCMAKE_BUILD_TYPE=Debug -DBUILD_JUCE_PLUGIN=OFF; \
	fi
	@cmake --build build/dev-vcv --target vcv-plugin-clean || true
	@cmake --build build/dev-vcv --target vcv-plugin -j$$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
	@cmake --build build/dev-vcv --target vcv-plugin-install
	@echo "VCV plugin cleaned, built, and installed"

# JUCE Plugin Development
juce: header
	@rm -rf build/dev-juce
	@if cmake --preset dev-juce; then \
		echo "Using CMake preset: dev-juce"; \
	else \
		echo "CMake preset not available, using manual configuration"; \
		cmake -B build/dev-juce -DCMAKE_BUILD_TYPE=Debug -DBUILD_VCV_PLUGIN=OFF; \
	fi
	@cmake --build build/dev-juce --target OctobIR_All -j$$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
	@echo "JUCE plugin cleaned and built: build/dev-juce/plugins/juce-multiformat/OctobIR_artefacts/"

# Core Library
core: header
	@rm -rf build/dev
	@if cmake --preset dev; then \
		echo "Using CMake preset: dev"; \
	else \
		echo "CMake preset not available, using manual configuration"; \
		cmake -B build/dev -DCMAKE_BUILD_TYPE=Debug; \
	fi
	@cmake --build build/dev --target octobir-core -j$$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
	@echo "Core library cleaned and built"

# Unit Tests
test:
	@rm -rf build/test
	@cmake -B build/test -DCMAKE_BUILD_TYPE=Debug -DBUILD_JUCE_PLUGIN=OFF -DBUILD_VCV_PLUGIN=OFF -DBUILD_TESTS=ON
	@cmake --build build/test --target octobir-core-tests -j$$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
	@echo "Running tests..."
	@./build/test/libs/octobir-core/tests/octobir-core-tests

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
	@find libs plugins -name "*.cpp" -o -name "*.hpp" -o -name "*.h" | \
		xargs clang-format --dry-run --Werror --style=file || \
		(echo "Error: Code is not properly formatted. Run 'make format' to fix." && exit 1)
	@echo "✓ Code formatting verified"
	@echo ""
	@echo "Running static analysis..."
	@if ! command -v clang-tidy &> /dev/null; then \
		echo "Error: clang-tidy not installed. Run ./scripts/setup-dev.sh"; \
		exit 1; \
	fi
	@find libs/octobir-core/src -name "*.cpp" | \
		xargs -I {} clang-tidy {} -- \
		-I./libs/octobir-core/include \
		-I./third_party/WDL/WDL \
		-I./third_party \
		-std=c++11 || \
		(echo "Error: Static analysis found issues" && exit 1)
	@if [ -n "$$RACK_DIR" ] && [ -d "$$RACK_DIR" ]; then \
		find plugins/vcv-rack/src -name "*.cpp" | \
			xargs -I {} clang-tidy {} -- \
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
	@echo "✓ Static analysis passed"
	@echo ""
	@echo "All code quality checks passed"

# Auto-format code
format:
	@echo "Formatting code..."
	@if ! command -v clang-format &> /dev/null; then \
		echo "Error: clang-format not installed. Run ./scripts/setup-dev.sh"; \
		exit 1; \
	fi
	@find libs plugins -name "*.cpp" -o -name "*.hpp" -o -name "*.h" | \
		xargs clang-format -i --style=file
	@echo "Code formatted"

# Install JUCE plugin (release build)
install-juce:
	@echo "Building and installing JUCE plugin (Release)..."
	@rm -rf build/release-juce
	@cmake -B build/release-juce \
		-DCMAKE_BUILD_TYPE=Release \
		-DBUILD_JUCE_PLUGIN=ON \
		-DBUILD_VCV_PLUGIN=OFF \
		-DBUILD_TESTS=OFF
	@cmake --build build/release-juce --target OctobIR_All --config Release -j$$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
	@echo ""
	@echo "Installing plugins to system directories..."
	@if [ -e ~/Library/Audio/Plug-Ins/VST3/OctobIR.vst3 ] && [ ! -w ~/Library/Audio/Plug-Ins/VST3/OctobIR.vst3 ]; then \
		echo ""; \
		echo "Error: Existing VST3 plugin is not writable (may be owned by root)."; \
		echo "Please remove it first:"; \
		echo "  sudo rm -rf ~/Library/Audio/Plug-Ins/VST3/OctobIR.vst3"; \
		echo "  sudo rm -rf ~/Library/Audio/Plug-Ins/Components/OctobIR.component"; \
		echo "Then run: make install-juce"; \
		echo ""; \
		exit 1; \
	fi
	@cp -rf build/release-juce/plugins/juce-multiformat/OctobIR_artefacts/Release/VST3/OctobIR.vst3 \
		~/Library/Audio/Plug-Ins/VST3/
	@echo "  ✓ VST3 installed"
	@cp -rf build/release-juce/plugins/juce-multiformat/OctobIR_artefacts/Release/AU/OctobIR.component \
		~/Library/Audio/Plug-Ins/Components/
	@echo "  ✓ AU installed"
	@echo ""
	@echo "✓ JUCE plugin installed:"
	@echo "  VST3: ~/Library/Audio/Plug-Ins/VST3/OctobIR.vst3"
	@echo "  AU:   ~/Library/Audio/Plug-Ins/Components/OctobIR.component"

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
	@cd plugins/vcv-rack && \
		export RACK_DIR=$${RACK_DIR:-../../../Rack} && \
		make clean && \
		make -j$$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4) && \
		make install
	@echo ""
	@echo "✓ VCV plugin installed"
