#!/bin/bash

set -e

echo "Setting up OctobIR development environment..."

echo "Initializing git submodules..."
git submodule update --init --recursive

if [ ! -d "third_party/JUCE" ]; then
    echo "Error: JUCE submodule not found"
    exit 1
fi

if [ ! -d "third_party/WDL" ]; then
    echo "Error: WDL submodule not found"
    exit 1
fi

echo "✓ Submodules initialized"

echo ""
echo "Installing required development tools..."

# Install pinned clang-format via pip for version consistency across platforms.
# The exact version must match ci.yml to prevent formatting divergence.
CLANG_FORMAT_VERSION="22.1.2"
echo "Installing clang-format ${CLANG_FORMAT_VERSION}..."
if command -v pipx &> /dev/null; then
    pipx install "clang-format==${CLANG_FORMAT_VERSION}"
    echo "✓ clang-format ${CLANG_FORMAT_VERSION} installed via pipx"
elif command -v pip3 &> /dev/null; then
    pip3 install --user "clang-format==${CLANG_FORMAT_VERSION}" --break-system-packages 2>/dev/null \
        || pip3 install --user "clang-format==${CLANG_FORMAT_VERSION}"
    echo "✓ clang-format ${CLANG_FORMAT_VERSION} installed via pip"
elif command -v pip &> /dev/null; then
    pip install --user "clang-format==${CLANG_FORMAT_VERSION}" --break-system-packages 2>/dev/null \
        || pip install --user "clang-format==${CLANG_FORMAT_VERSION}"
    echo "✓ clang-format ${CLANG_FORMAT_VERSION} installed via pip"
else
    echo "Error: pipx or pip is required to install clang-format"
    echo "Install pipx with: brew install pipx"
    exit 1
fi

# Check for clang-tidy
if ! command -v clang-tidy &> /dev/null; then
    echo "Installing clang-tidy..."
    if [[ "$OSTYPE" == "darwin"* ]]; then
        brew install llvm
        echo "Note: You may need to add LLVM to your PATH:"
        echo "  export PATH=\"/opt/homebrew/opt/llvm/bin:\$PATH\""
    elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
        sudo apt-get install -y clang-tidy
    else
        echo "Error: Unsupported platform for automatic installation"
        echo "Please manually install clang-tidy"
        exit 1
    fi
else
    echo "✓ clang-tidy already installed"
fi

# Verify both tools are now available
if ! command -v clang-format &> /dev/null || ! command -v clang-tidy &> /dev/null; then
    echo ""
    echo "Error: Required tools are not available after installation"
    echo "Please install manually:"
    echo "  - clang-format: Code formatting"
    echo "  - clang-tidy: Static analysis"
    exit 1
fi

echo "✓ Code quality tools installed"

# Check for reuse (FSFE REUSE license compliance tool)
if ! command -v reuse &> /dev/null; then
    echo "Installing reuse (FSFE license compliance tool)..."
    if command -v pipx &> /dev/null; then
        pipx install reuse
    elif command -v pip3 &> /dev/null; then
        pip3 install --user reuse
    elif command -v pip &> /dev/null; then
        pip install --user reuse
    else
        echo "Warning: Could not install reuse (no pip/pipx found)"
        echo "Install manually: pip install reuse"
        echo "Continuing without it..."
    fi
else
    echo "✓ reuse already installed"
fi

echo "✓ All development tools installed"

echo ""
echo "Installing git hooks..."
if [ ! -d ".git" ]; then
    echo "Error: .git directory not found. Are you in a git repository?"
    exit 1
fi

if [ ! -d ".git/hooks" ]; then
    echo "Creating .git/hooks directory..."
    mkdir -p .git/hooks
fi

if [ -f "scripts/hooks/pre-commit" ]; then
    cp scripts/hooks/pre-commit .git/hooks/pre-commit
    chmod +x .git/hooks/pre-commit
    echo "✓ Pre-commit hook installed"
else
    echo "Error: pre-commit hook not found at scripts/hooks/pre-commit"
    echo "Expected path: $(pwd)/scripts/hooks/pre-commit"
    exit 1
fi

echo ""
echo "Setup complete! Next steps:"
echo ""
echo "  Build and install plugins:"
echo "    make juce       # Build and install JUCE plugin (VST3 + AU)"
echo "    make vcv        # Build and install VCV plugin"
echo ""
echo "  Run tests:"
echo "    make test       # Core library tests"
echo "    make test-juce  # JUCE plugin tests"
echo "    make test-vcv   # VCV plugin tests"
echo ""
echo "  Run 'make help' for all available targets."
echo ""
