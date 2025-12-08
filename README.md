# UCX Avalonia App - Modern Cross-Platform UI for u-connectXpress

A modern cross-platform application for testing and configuring u-connectXpress devices (NORA-W36, NORA-B26). Built with Avalonia UI (.NET 9) and a native C wrapper around the ucxclient library.

## Architecture

```
┌─────────────────────────────────┐
│   Avalonia UI (.NET 9)          │
│   - Modern MVVM pattern         │
│   - Cross-platform (Win/Linux)  │
└────────────┬────────────────────┘
             │ P/Invoke
┌────────────▼────────────────────┐
│   ucxclient_wrapper.dll         │
│   - C API exports               │
│   - Auto-generated wrapper      │
└────────────┬────────────────────┘
             │
┌────────────▼────────────────────┐
│   ucxclient library             │
│   - AT client & URC handling    │
│   - UART abstraction            │
│   - UCX API (365+ functions)    │
└─────────────────────────────────┘
```

## Prerequisites

### System Requirements
- **Windows 10 or Windows 11** (64-bit) - Linux/macOS support planned
- **FTDI USB device** (NORA-W36 or NORA-B26 module)

### Required Software
1. **.NET 9.0 SDK**
   - Download: https://dotnet.microsoft.com/download/dotnet/9.0
   - Required for Avalonia UI application

2. **Visual Studio 2022 Build Tools** (or full Visual Studio 2022)
   - Download: https://aka.ms/vs/17/release/vs_BuildTools.exe
   - Install "Desktop development with C++" workload
   - Required for building native wrapper DLL

3. **CMake 3.20 or later**
   - Download: https://cmake.org/download/
   - Add to system PATH during installation

4. **Git**
   - Download: https://git-scm.com/download/win

## Quick Start

```powershell
# Clone repository with ucxclient submodule
git clone --recurse-submodules=ucxclient https://github.com/u-blox/ucx-windows-app.git
cd ucx-avalonia-app

# Launch (auto-builds on first run)
.\launch-ucx-avalonia-app.cmd
```

## What the Launch Script Does

The launcher (`launch-ucx-avalonia-app.cmd`) automates the entire build and run process:

1. ✅ Checks for .NET SDK and CMake
2. ✅ Builds native wrapper DLL with auto-generated UCX API bindings
3. ✅ Copies DLL to Debug and Release output directories
4. ✅ Builds the Avalonia .NET application
5. ✅ Runs the application

```powershell
# Run application (auto-builds everything)
.\launch-ucx-avalonia-app.cmd
```

## Key Features

### Current Implementation
- ✅ **COM Port Management**: Auto-detection and selection of FTDI devices
- ✅ **WiFi Scanning**: Scan for networks with SSID, BSSID, channel, RSSI, security type
- ✅ **WiFi Connection**: Connect to networks and retrieve IP configuration
- ✅ **Network Status**: Real-time network up/down events via URCs
- ✅ **AT Command Interface**: Send custom AT commands with response logging
- ✅ **Modern UI**: Clean, responsive Avalonia interface with MVVM architecture
- ✅ **Auto-Generated API**: 365+ UCX API functions available through wrapper

### Planned Features
- 🔄 **Bluetooth**: Scan, connect, bond, SPS, GATT operations
- 🔄 **Network Services**: Socket, HTTP, MQTT client
- 🔄 **Firmware Updates**: XMODEM protocol integration
- 🔄 **Profile Management**: Save and restore WiFi/BT configurations
- 🔄 **Cross-Platform**: Linux and macOS support

## Project Structure

```
ucx-avalonia-app/
├── launch-ucx-avalonia-app.cmd     # Windows launcher script
├── README.md
│
├── ucx-avalonia-app/               # Avalonia .NET application
│   ├── Program.cs                  # Entry point
│   ├── App.axaml                   # Application definition
│   ├── UcxAvaloniaApp.csproj       # .NET project file
│   ├── Services/
│   │   ├── UcxNative.cs            # P/Invoke declarations
│   │   ├── UcxClient.cs            # Managed C# wrapper
│   │   └── SerialPortService.cs    # COM port enumeration
│   ├── ViewModels/
│   │   └── MainWindowViewModel.cs  # MVVM view model
│   └── Views/
│       └── MainWindow.axaml        # UI layout
│
├── ucxclient-wrapper/              # Native C DLL wrapper
│   ├── CMakeLists.txt              # Build configuration
│   ├── generate_wrapper.py         # Auto-generates API bindings
│   ├── ucxclient_wrapper.h         # Public C API header
│   ├── ucxclient_wrapper_core.c    # Core wrapper implementation
│   ├── ucxclient_wrapper_generated.c   # Generated UCX API wrapper
│   └── u_port_windows.c            # Windows UART port implementation
│
└── ucxclient/                      # Git submodule
    ├── src/                        # UCX client library
    ├── inc/                        # Headers
    ├── ucx_api/                    # UCX API definitions (365+ functions)
    └── ports/                      # Platform abstraction
```

## Manual Building

### Build Native Wrapper DLL

```powershell
cd ucxclient-wrapper
mkdir build
cd build
cmake -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config Release
```

Output: `ucxclient-wrapper/build/bin/Release/ucxclient_wrapper.dll`

### Build Avalonia Application

```powershell
cd ucx-avalonia-app
dotnet build -c Release
```

### Copy DLL to Output

```powershell
copy ucxclient-wrapper\build\bin\Release\ucxclient_wrapper.dll ucx-avalonia-app\bin\Release\net9.0\
```

### Run

```powershell
cd ucx-avalonia-app
dotnet run -c Release
```

## Auto-Generated Wrapper

The native wrapper includes **365+ UCX API functions** auto-generated from the ucxclient library:

- `generate_wrapper.py` - Parses ucx_api headers and generates C/C# bindings
- `ucxclient_wrapper_generated.c` - Auto-generated C API exports
- `Services/UcxNative.cs` - Auto-generated P/Invoke declarations

**Core Functions** (manually implemented):
- `ucx_create()` / `ucx_destroy()` - Instance management
- `ucx_set_urc_callback()` - Unsolicited result code handling
- `ucx_get_error_string()` - Error message retrieval

**Generated API** (examples):
- `uCxWifiStationScan1Begin()` / `uCxWifiStationScan1GetNext()`
- `uCxWifiStationSetConnectionParams6()`
- `uCxWifiStationConnect2()`
- `uCxWifiStationGetNetworkStatus1()`

See `ucxclient-wrapper/CODEGEN_README.md` for details on the code generation process.

---

**Version**: 1.0.0 (Proof of Concept)  
**Platform**: Windows 10/11 (64-bit) | Linux/macOS (planned)  
**UCX API**: v3.2.0+  



# Disclaimer
Copyright &#x00a9; u-blox

u-blox reserves all rights in this deliverable (documentation, software, etc.,
hereafter “Deliverable”).

u-blox grants you the right to use, copy, modify and distribute the
Deliverable provided hereunder for any purpose without fee.

THIS DELIVERABLE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR IMPLIED
WARRANTY. IN PARTICULAR, NEITHER THE AUTHOR NOR U-BLOX MAKES ANY
REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE MERCHANTABILITY OF THIS
DELIVERABLE OR ITS FITNESS FOR ANY PARTICULAR PURPOSE.

In case you provide us a feedback or make a contribution in the form of a
further development of the Deliverable (“Contribution”), u-blox will have the
same rights as granted to you, namely to use, copy, modify and distribute the
Contribution provided to us for any purpose without fee.
