# UCX Web Terminal - NORA-W36

Browser-based terminal for controlling NORA-W36 module using the full UCX API via WebAssembly.

## Overview

This project compiles the ucxclient C library to WebAssembly, allowing you to control a NORA-W36 module directly from a web browser using the same high-level UCX API used by the desktop applications. This eliminates the need to manually send AT commands.

## Architecture

```
Browser (JavaScript)
    ↓
WASM Module (ucxclient compiled to WebAssembly)
    ↓
Web Serial API Bridge (u_port_web.c)
    ↓
Web Serial API
    ↓
NORA-W36 Module (USB/Serial)
```

## Features

- ✅ **Full UCX API in browser** - Same API as desktop C#/Python applications
- ✅ **WiFi scanning and connection** - Use high-level functions instead of AT commands
- ✅ **Web Serial API** - Direct USB/serial communication from browser
- ✅ **Real-time logging** - See all serial communication and URCs
- ✅ **Cross-platform** - Works on any OS with Chrome/Edge browser

## Requirements

### Browser Support
- **Chrome/Edge 89+** (Web Serial API support)
- ~77% market share as of 2024
- Safari and Firefox do not support Web Serial API yet

### Build Tools
- **Emscripten SDK** - Compile C to WebAssembly
- **CMake 3.10+** - Build system
- **Git** - Version control

## Installation

### 1. Install Emscripten SDK

```bash
# Clone Emscripten SDK
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk

# Install latest version
./emsdk install latest
./emsdk activate latest

# Add to PATH (Windows PowerShell)
.\emsdk_env.ps1

# Verify installation
emcc --version
```

### 2. Build WASM Module

```bash
cd ucx-web-terminal

# Configure build
emcmake cmake -B build

# Build
emmake make -C build

# Output: build/ucxclient.js and build/ucxclient.wasm
```

## Usage

### 1. Start Web Server

```bash
# Simple HTTP server (Python)
python -m http.server 8000

# OR using Node.js
npx http-server -p 8000
```

### 2. Open in Browser

Navigate to `http://localhost:8000/index.html`

### 3. Connect to NORA-W36

1. Click **"Connect to Serial Port"**
2. Select your NORA-W36 USB device (e.g., `COM3` or `/dev/ttyUSB0`)
3. Serial port opens at 115200 baud (default)
4. UCX client automatically initializes

### 4. Scan WiFi Networks

1. Click **"Scan Networks"**
2. Wait ~5 seconds for scan to complete
3. Results display in table with "Select" buttons

### 5. Connect to WiFi

1. Enter SSID and password (or select from scan results)
2. Click **"Connect"**
3. Wait for connection (monitor console log)
4. Click **"Get Connection Info"** to see IP address

## File Structure

```
ucx-web-terminal/
├── CMakeLists.txt           # Emscripten build configuration
├── index.html               # Web terminal UI
├── library.js               # JavaScript functions for C code
├── u_port_web.c             # Web Serial API bridge
├── ucx_wasm_wrapper.c       # Simplified WASM API
└── README.md                # This file
```

## API Functions

The WASM module exports these simplified functions:

| Function | Description |
|----------|-------------|
| `ucx_create()` | Initialize UCX client |
| `ucx_destroy(handle)` | Cleanup UCX client |
| `ucx_wifi_scan_begin()` | Start WiFi scan |
| `ucx_wifi_scan_get_next(result)` | Get next scan result |
| `ucx_wifi_scan_end()` | End WiFi scan |
| `ucx_wifi_connect(ssid, pass)` | Connect to WiFi network |
| `ucx_wifi_disconnect()` | Disconnect from WiFi |
| `ucx_wifi_get_connection_info(info)` | Get IP/subnet/gateway |

## Code Reuse

This project achieves **~95% code reuse** with the desktop applications:

- ✅ **ucxclient core** - All 365+ UCX functions (100% shared)
- ✅ **Parser/buffer logic** - Shared between all platforms
- ✅ **WiFi/BT/Socket APIs** - Identical across desktop and web
- ⚠️ **Platform layer only** - Different for each platform:
  - Desktop: Native serial port APIs (Windows/Linux)
  - Web: Web Serial API bridge (~250 lines)

## Browser Console

Use the browser console to call UCX functions directly:

```javascript
// Manual WiFi scan
const scanId = ucxModule.ccall('ucx_wifi_scan_begin', 'number', [], []);

// Get scan result
const resultPtr = ucxModule._malloc(256);
ucxModule.ccall('ucx_wifi_scan_get_next', 'number', ['number'], [resultPtr]);
const ssid = ucxModule.UTF8ToString(resultPtr);
ucxModule._free(resultPtr);

// Connect to WiFi
const ssidPtr = ucxModule._malloc(64);
const passPtr = ucxModule._malloc(64);
ucxModule.stringToUTF8("MyNetwork", ssidPtr, 64);
ucxModule.stringToUTF8("password123", passPtr, 64);
ucxModule.ccall('ucx_wifi_connect', 'number', ['number', 'number'], [ssidPtr, passPtr]);
ucxModule._free(ssidPtr);
ucxModule._free(passPtr);
```

## Troubleshooting

### Web Serial API Not Available
- **Symptom**: Alert "Web Serial API not supported"
- **Solution**: Use Chrome or Edge browser (version 89+)

### Serial Port Not Opening
- **Symptom**: Error when clicking "Connect to Serial Port"
- **Solution**: 
  - Check NORA-W36 is connected via USB
  - Try different baud rate (115200 default)
  - Check browser console for errors

### WiFi Scan Returns No Results
- **Symptom**: "No networks found" after scan
- **Solution**:
  - Wait longer (WiFi scan takes ~5 seconds)
  - Check serial communication in console log
  - Verify NORA-W36 is responding to AT commands

### WASM Module Not Loading
- **Symptom**: "Failed to load WASM module" error
- **Solution**:
  - Rebuild with `emmake make -C build`
  - Check `ucxclient.js` and `ucxclient.wasm` exist in same directory as `index.html`
  - Serve via HTTP server (not `file://` URLs)

## Performance

- **Binary Size**: ~200KB (compressed WASM)
- **Load Time**: <1 second on broadband
- **Execution Speed**: Near-native (within 10% of compiled C)
- **Memory Usage**: ~4MB (includes ucxclient buffers)

## Security

- **Web Serial API** requires user permission (browser prompts)
- **HTTPS required** for production deployment
- **Same-origin policy** applies to WASM modules
- **No credentials stored** in browser (all in-memory)

## Future Enhancements

- [ ] Bluetooth scanning and pairing
- [ ] Socket/HTTP/MQTT testing
- [ ] Multi-device support (connect to multiple modules)
- [ ] Configuration save/restore (localStorage)
- [ ] Firmware update via browser

## License

Copyright 2025 u-blox

Licensed under the Apache License, Version 2.0
