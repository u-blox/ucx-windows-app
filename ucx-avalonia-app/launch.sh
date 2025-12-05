#!/bin/bash
# Cross-platform launcher for UCX Avalonia App (Linux/macOS)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "UCX Avalonia App Launcher"
echo "========================="
echo ""

# Detect platform
if [[ "$OSTYPE" == "darwin"* ]]; then
    PLATFORM="macOS"
    LIB_NAME="libucxclient_wrapper.dylib"
    LIB_SRC="../build-wrapper/bin/libucxclient_wrapper.dylib"
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    PLATFORM="Linux"
    LIB_NAME="libucxclient_wrapper.so"
    LIB_SRC="../build-wrapper/bin/libucxclient_wrapper.so"
else
    echo "ERROR: Unsupported platform: $OSTYPE"
    exit 1
fi

echo "Platform: $PLATFORM"
echo ""

# Check if .NET is installed
if ! command -v dotnet &> /dev/null; then
    echo "ERROR: .NET SDK not found!"
    echo "Please install .NET 9.0 SDK or later from:"
    echo "https://dotnet.microsoft.com/download"
    exit 1
fi

DOTNET_VERSION=$(dotnet --version)
echo "✓ .NET SDK: $DOTNET_VERSION"

# Check if native library exists
if [ ! -f "$LIB_SRC" ]; then
    echo ""
    echo "WARNING: Native library not found: $LIB_SRC"
    echo "You need to build the native wrapper first."
    echo ""
    echo "Build instructions:"
    echo "  cd ../ucxclient-wrapper"
    echo "  mkdir -p build && cd build"
    echo "  cmake .."
    echo "  cmake --build . --config Release"
    echo ""
    read -p "Continue anyway? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
else
    echo "✓ Native library: $LIB_NAME"
    
    # Copy native library to output directory
    TARGET_DIR="bin/Debug/net9.0"
    mkdir -p "$TARGET_DIR"
    cp "$LIB_SRC" "$TARGET_DIR/"
    echo "✓ Copied $LIB_NAME to $TARGET_DIR"
fi

echo ""
echo "Building and running application..."
echo ""

# Build and run
dotnet build
dotnet run
