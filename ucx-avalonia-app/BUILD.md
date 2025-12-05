# Building UCX Avalonia App (Cross-Platform)

This application runs on Windows, Linux, and macOS.

## Prerequisites

### All Platforms
- .NET 9.0 SDK or later
- CMake 3.20 or later
- Git

### Windows
- Visual Studio 2022 (with C++ workload) or
- Visual Studio Build Tools 2022

### Linux
- GCC or Clang compiler
- Build essentials: `sudo apt install build-essential cmake`

### macOS
- Xcode Command Line Tools: `xcode-select --install`

## Building the Native Library

### Windows

```powershell
# Navigate to wrapper directory
cd ucxclient-wrapper

# Create build directory
mkdir build
cd build

# Configure and build (Release)
cmake ..
cmake --build . --config Release

# Copy DLL to .NET app
copy bin\Release\ucxclient_wrapper.dll ..\ucx-avalonia-app\bin\Debug\net9.0\
```

### Linux

```bash
# Navigate to wrapper directory
cd ucxclient-wrapper

# Create build directory
mkdir -p build
cd build

# Configure and build
cmake ..
cmake --build . --config Release

# Copy .so to .NET app
cp bin/libucxclient_wrapper.so ../ucx-avalonia-app/bin/Debug/net9.0/
```

### macOS

```bash
# Navigate to wrapper directory
cd ucxclient-wrapper

# Create build directory
mkdir -p build
cd build

# Configure and build
cmake ..
cmake --build . --config Release

# Copy .dylib to .NET app
cp bin/libucxclient_wrapper.dylib ../ucx-avalonia-app/bin/Debug/net9.0/
```

## Building the Avalonia App

### All Platforms

```bash
# Navigate to Avalonia app directory
cd ucx-avalonia-app

# Restore dependencies
dotnet restore

# Build
dotnet build

# Run
dotnet run
```

## Serial Port Names

The application will detect serial ports automatically based on the platform:

- **Windows**: `COM1`, `COM2`, `COM11`, etc.
- **Linux**: `/dev/ttyUSB0`, `/dev/ttyACM0`, etc.
- **macOS**: `/dev/cu.usbserial-*`, `/dev/cu.usbmodem-*`, etc.

## Troubleshooting

### Linux: Permission Denied on Serial Port

```bash
# Add your user to the dialout group
sudo usermod -a -G dialout $USER

# Log out and log back in for changes to take effect
```

### macOS: Library Not Loaded

```bash
# Ensure the .dylib is in the same directory as the executable
# Or add to DYLD_LIBRARY_PATH:
export DYLD_LIBRARY_PATH=$DYLD_LIBRARY_PATH:$(pwd)/bin/Debug/net9.0
```

### All Platforms: Native Library Not Found

Make sure the native library is in the same directory as the executable:
- Windows: `ucxclient_wrapper.dll`
- Linux: `libucxclient_wrapper.so`
- macOS: `libucxclient_wrapper.dylib`

## Publishing for Distribution

### Windows

```powershell
dotnet publish -c Release -r win-x64 --self-contained
# Output: bin\Release\net9.0\win-x64\publish\
```

### Linux

```bash
dotnet publish -c Release -r linux-x64 --self-contained
# Output: bin/Release/net9.0/linux-x64/publish/
```

### macOS (Intel)

```bash
dotnet publish -c Release -r osx-x64 --self-contained
# Output: bin/Release/net9.0/osx-x64/publish/
```

### macOS (Apple Silicon)

```bash
dotnet publish -c Release -r osx-arm64 --self-contained
# Output: bin/Release/net9.0/osx-arm64/publish/
```

Remember to copy the native library to the publish directory!
