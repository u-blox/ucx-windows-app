# UCX Avalonia App - Proof of Concept

This is a proof-of-concept implementation demonstrating how to build a modern cross-platform UI for ucxclient using Avalonia and a native C DLL wrapper.

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
│   - Simple wrapper layer        │
└────────────┬────────────────────┘
             │
┌────────────▼────────────────────┐
│   ucxclient library             │
│   - AT client & URC handling    │
│   - UART abstraction            │
│   - Protocol logic              │
└─────────────────────────────────┘
```

## Project Structure

- **ucxclient-wrapper/** - Native C DLL wrapper
  - `ucxclient_wrapper.h` - C API header
  - `ucxclient_wrapper.c` - Implementation
  - Uses existing `ucxclient` library and Windows port
  
- **ucx-avalonia-app/** - Avalonia .NET application
  - `Services/UcxNative.cs` - P/Invoke declarations
  - `Services/UcxClient.cs` - Managed C# wrapper
  - `Services/SerialPortService.cs` - COM port enumeration
  - `ViewModels/MainWindowViewModel.cs` - MVVM view model
  - `Views/MainWindow.axaml` - UI layout

## Building

### 1. Build the native DLL:

```powershell
cd ucx-windows-app
mkdir build-wrapper -Force
cd build-wrapper
cmake -G "Visual Studio 17 2022" -A x64 ../ucxclient-wrapper
cmake --build . --config Release
```

The DLL will be in: `build-wrapper/bin/Release/ucxclient_wrapper.dll`

### 2. Build the Avalonia app:

```powershell
cd ucx-windows-app/ucx-avalonia-app
dotnet build
```

### 3. Copy the DLL to the output:

```powershell
copy ..\build-wrapper\bin\Release\ucxclient_wrapper.dll bin\Debug\net9.0\
```

## Running

```powershell
cd ucx-avalonia-app
dotnet run
```

Or in Release mode:
```powershell
dotnet run --configuration Release
```

## Features (PoC)

- ✅ COM port selection and refresh
- ✅ Connect/disconnect to NORA-W36 module
- ✅ Send AT commands with response logging
- ✅ URC (Unsolicited Result Code) handling
- ✅ Timestamped log viewer
- ✅ Native ucxclient integration
- ✅ **WiFi scan using ucx_api functions**
  - Displays SSID, BSSID, channel, RSSI
  - Shows security type (Open/WPA/WPA2/WPA3)
  - Results displayed in sortable DataGrid

## What Works

- The app successfully loads and displays available COM ports
- Connects to a NORA-W36 module using the native ucxclient library
- Sends AT commands through the wrapper DLL
- Receives and displays URCs in real-time
- **Performs WiFi scans using `uCxWifiStationScan1Begin/GetNext`**
- **Parses security information (authentication suites, ciphers)**
- Clean MVVM architecture with proper separation of concerns

## Next Steps

To build a full-featured application, consider:

1. **Higher-level APIs** - Wrap specific ucx_api functions (WiFi scan, BT operations, etc.)
2. **Tab-based UI** - Separate tabs for WiFi, Bluetooth, System, etc.
3. **Settings persistence** - Save last used port, baud rate
4. **Better response parsing** - Extract and display AT response parameters
5. **Error handling** - Improve error messages and recovery
6. **Auto-reconnect** - Handle connection drops gracefully
7. **Firmware update UI** - Integrate XMODEM transfer
8. **Cross-platform testing** - Test on Linux/macOS

## Advantages of This Approach

✅ **Code Reuse** - Leverages all existing ucxclient AT parsing and protocol logic  
✅ **Performance** - Native C for UART/protocol, managed C# for UI  
✅ **Modern UI** - Avalonia provides clean, cross-platform interface  
✅ **Maintainability** - Clear separation: native (device) vs managed (UI)  
✅ **Portability** - Can run on Windows, Linux, macOS with minimal changes  

## Dependencies

- .NET 9.0 SDK
- CMake 3.20+
- Visual Studio 2022 (or compatible C compiler)
- Avalonia 11.3.9
- CommunityToolkit.Mvvm 8.2.1

## License

Same as ucxclient - Apache License 2.0
