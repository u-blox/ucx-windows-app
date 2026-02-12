# UCX Web App - NORA-W36 WebAssembly Interface

Browser-based application for controlling u-blox NORA-W36 WiFi/Bluetooth modules using the full UCX API compiled to WebAssembly.

## Overview

This is a WebAssembly port of the ucxclient C library, enabling direct control of NORA-W36 modules from any modern web browser. It provides the same high-level UCX API used by desktop applications (Windows/Linux/Python), but runs entirely in the browser using Web Serial API for communication.

**Key Innovation**: The complete ucxclient C library is compiled to WASM, achieving ~95% code reuse with desktop applications.

## Architecture

```
+--------------------------------------------------+
|  Web Browser (Chrome/Edge 89+)                   |
|  +--------------------------------------------+  |
|  |  index.html  (markup + styles)             |  |
|  |    +- serial.js      SerialManager class   |  |
|  |    +- wasm-bridge.js WasmBridge class       |  |
|  |    +- app.js         UI logic & URC        |  |
|  +------------------+-------------------------+  |
|                     | ccall / Module hooks        |
|  +------------------v-------------------------+  |
|  |  ucxclient.wasm  (Emscripten + ASYNCIFY)   |  |
|  |  +--------------------------------------+  |  |
|  |  |  ucxclient C library (26 src files)  |  |  |
|  |  |  WiFi, Bluetooth, HTTP, MQTT, Socket |  |  |
|  |  +-----------------+-------------------+|  |  |
|  |  +-----------------v-------------------+|  |  |
|  |  |  ucx_wasm_wrapper.c  (33 exports)   ||  |  |
|  |  |  u_port_web.c  (EM_JS serial bridge)||  |  |
|  |  +-----------------+-------------------+|  |  |
|  +---------------------+------------------+  |  |
|                        | Module.serialWrite/Read |
|  +---------------------v------------------+  |  |
|  |  Web Serial API                        |  |  |
|  +---------------------+------------------+  |  |
+------------------------|--------------------------+
                         | USB/Serial
                +--------v---------+
                |  NORA-W36 Module |
                +------------------+
```

## Features

- **Full ucxclient API** - WiFi, Bluetooth, HTTP, MQTT, sockets compiled to WASM
- **WiFi** - Scan networks, connect/disconnect, DHCP, IP address retrieval
- **BLE Scan** - Foreground discovery with configurable timeout, RSSI, device names
- **BLE Connect** - Connect/disconnect to BLE peripherals with connection handle management
- **GATT Client** - Discover services and characteristics, read/write values, enable notifications/indications
- **GATT Server** - Define services and characteristics, activate, set attribute values, send notifications
- **BLE Advertising** - Start/stop legacy BLE advertising
- **Modular JavaScript** - Clean separation: `serial.js`, `wasm-bridge.js`, `app.js`
- **Web Serial API** - Direct USB/serial with 16 KB ring buffer and write queue
- **Runtime log levels** - Configurable via `ucx_set_log_level` (none/error/info/debug)
- **Safe memory access** - `ucx_read_int32` C helper avoids ASYNCIFY heap-view staleness
- **Auto-reconnect** - Remembers previously paired serial ports
- **Disconnect detection** - Monitors serial port and cleans up on unplug
- **RX line reassembly** - Accumulates serial chunks, displays complete lines
- **URC handling** - Processes WiFi (+UEWLU, +UEWSNU, +UEWSND) and BLE (+UEBTC, +UEBTDC, +UEBTGCN, +UEBTGSW) events
- **Cross-platform** - Windows/Mac/Linux with Chrome/Edge 89+
- **One-click launcher** - `launch-ucx-web-app.cmd` builds and serves

## Quick Start

### Prerequisites

- **Chrome or Edge browser** (version 89+) with Web Serial API support
- **NORA-W36 module** connected via USB
- **Windows with Git and Python** (tested on Windows 10/11)

### Installation & Running

1. **Clone the repository with submodules**:
   ```bash
   git clone --recursive https://github.com/u-blox/ucx-windows-app.git ucx-web-app
   cd ucx-web-app
   ```

2. **Run the launcher** (Windows):
   ```cmd
   launch-ucx-web-app.cmd
   ```

   This script will:
   - Initialize and activate Emscripten SDK (included as submodule)
   - Initialize the ucxclient submodule
   - Configure CMake with Emscripten toolchain
   - Build 26 C source files to WASM (link step uses Python to work around Windows `emcc.bat` bugs)
   - Copy `ucxclient.js` and `ucxclient.wasm` into `ucx-web-app/`
   - Start Python HTTP server on port 8000
   - Open browser to `http://localhost:8000/ucx-web-app/index.html`

3. **Use the web interface**:
   - The app will auto-connect to a previously paired serial port
   - Otherwise click "Connect to Serial Port" and select your NORA-W36 device
   - Click "Scan Networks" to detect WiFi networks
   - Enter credentials and click "Connect" to join a network
   - Click "Get Connection Info" to see the assigned IP address
   - Click "BLE Scan" to discover nearby Bluetooth devices
   - Select a device and click "Connect" to establish a BLE connection
   - Use GATT Client to discover services, read/write characteristics, enable notifications
   - Use GATT Server to define services/characteristics and advertise

### Manual Build (Linux/Mac)

```bash
cd ucx-web-app

# Activate Emscripten
source ../emsdk/emsdk_env.sh

# Configure and build
emcmake cmake -B build -DCMAKE_BUILD_TYPE=Release
emmake make -C build

# Copy outputs
cp build/ucxclient.js build/ucxclient.wasm .

# Start web server
cd ..
python3 -m http.server 8000
# Open http://localhost:8000/ucx-web-app/index.html
```

## Project Structure

```
ucx-web-app/                         # repository root
+-- README.md                        # This file
+-- .gitignore                       # Ignore build artifacts, WASM binaries, *.bak
+-- .gitmodules                      # Submodules (emsdk, ucxclient)
+-- launch-ucx-web-app.cmd           # Windows one-click build & serve
+-- ucx-web-app.code-workspace       # VS Code workspace
+-- ucx-web-app.ini                  # User settings (not in git)
|
+-- emsdk/                           # Emscripten SDK (submodule)
|
+-- ucxclient/                       # ucxclient C library (submodule)
|   +-- src/                         # AT client, URC queue, XMODEM
|   +-- ucx_api/                     # Generated UCX API (NORA-W36X target)
|   +-- inc/                         # Public headers
|   +-- ports/                       # Platform-specific code
|
+-- ucx-web-app/                     # WebAssembly application
    +-- README.md                    # Implementation details
    +-- CMakeLists.txt               # Emscripten build configuration
    +-- index.html                   # Markup + styles (loads JS modules)
    +-- serial.js                    # SerialManager class + RingBuffer
    +-- wasm-bridge.js               # WasmBridge class (typed async API)
    +-- app.js                       # UI logic, URC handling, RX reassembly
    +-- library.js                   # Emscripten --js-library stub
    +-- u_port_web.c                 # Platform layer (EM_JS serial bridge)
    +-- u_port_clib.h                # Platform header (mutex no-ops)
    +-- ucx_wasm_wrapper.c           # WASM-exported C API (33 functions)
    +-- ucxclient.js                 # Generated WASM loader (not in git)
    +-- ucxclient.wasm               # Compiled WASM binary (not in git)
    +-- images/                      # Icons
    +-- build/                       # CMake build directory (not in git)
```

## Technical Details

### JavaScript Module Architecture

| File | Class / Role | Responsibility |
|------|-------------|----------------|
| `serial.js` | `SerialManager`, `RingBuffer` | Web Serial connection, 16 KB ring buffer, write queue, disconnect detection |
| `wasm-bridge.js` | `WasmBridge` | Loads WASM module, provides typed async API (WiFi, BLE scan, GATT client/server) |
| `app.js` | (functions) | DOM events, URC dispatcher, RX line reassembly, logging |
| `library.js` | (empty stub) | Required by `--js-library` flag; all bindings are EM_JS in `u_port_web.c` |

### Code Reuse with Desktop Applications

| Component | Shared | Platform-Specific |
|-----------|--------|-------------------|
| **ucxclient core** | 100% | - |
| **AT client/parser** | 100% | - |
| **UCX API functions** | 100% | - |
| **WiFi/BT/Socket APIs** | 100% | - |
| **Serial I/O** | - | `u_port_web.c` (~370 lines) |
| **Threading/mutex** | - | No-op stubs (`u_port_clib.h`) |

### Exported WASM Functions

The link step exports these C functions for JavaScript via `ccall`:

| Function | Parameters | Returns | Description |
|----------|-----------|---------|-------------|
| `ucx_init` | `portName, baudRate` | `int` | Initialize UCX client (0 = success) |
| `ucx_deinit` | - | `void` | Clean up UCX client |
| `ucx_set_log_level` | `level` | `void` | 0 = none, 1 = error, 2 = info, 3 = debug |
| `ucx_read_int32` | `int* ptr` | `int` | Safe memory read (avoids ASYNCIFY stale views) |
| `ucx_wifi_scan_begin` | - | `int` | Start active WiFi scan |
| `ucx_wifi_scan_get_next` | `ssid*, rssi*, channel*` | `int` | 1 = available, 0 = done, -1 = error |
| `ucx_wifi_scan_end` | - | `void` | End scan session |
| `ucx_wifi_connect` | `ssid, password` | `int` | Connect to WiFi network |
| `ucx_wifi_disconnect` | - | `int` | Disconnect from current network |
| `ucx_wifi_get_ip` | `ip*` | `int` | Get current IPv4 address |
| `ucx_send_at_command` | `cmd, response, size` | `int` | Send raw AT command |
| `ucx_get_version` | `version*, size` | `int` | Get module firmware version |
| `ucx_get_last_error` | - | `char*` | Get last error message |
| **Bluetooth** | | | |
| `ucx_bt_discovery_begin` | `timeout_ms` | `int` | Start BLE discovery |
| `ucx_bt_discovery_get_next` | `addr*, rssi*, name*` | `int` | 1 = device available, 0 = done |
| `ucx_bt_discovery_end` | - | `void` | End BLE discovery |
| `ucx_bt_connect` | `addr_str, conn_handle*` | `int` | Connect to BLE peripheral |
| `ucx_bt_disconnect` | `conn_handle` | `int` | Disconnect BLE connection |
| `ucx_bt_advertise_start` | - | `int` | Start legacy BLE advertising |
| `ucx_bt_advertise_stop` | - | `int` | Stop BLE advertising |
| **GATT Client** | | | |
| `ucx_gatt_discover_services_begin` | `conn_handle` | `int` | Start primary service discovery |
| `ucx_gatt_discover_services_get_next` | `start*, end*, uuid*` | `int` | 1 = service available, 0 = done |
| `ucx_gatt_discover_services_end` | - | `void` | End service discovery |
| `ucx_gatt_discover_chars_begin` | `conn, start, end` | `int` | Start characteristic discovery |
| `ucx_gatt_discover_chars_get_next` | `attr*, val*, props*, uuid*` | `int` | 1 = char available, 0 = done |
| `ucx_gatt_discover_chars_end` | - | `void` | End char discovery |
| `ucx_gatt_read` | `conn, val_handle, buf, max` | `int` | Read characteristic (returns byte count) |
| `ucx_gatt_write` | `conn, val_handle, data, len` | `int` | Write characteristic (with response) |
| `ucx_gatt_config_write` | `conn, cccd, config` | `int` | Enable notifications/indications |
| **GATT Server** | | | |
| `ucx_gatt_server_service_define` | `uuid, uuid_len, handle*` | `int` | Define a GATT service |
| `ucx_gatt_server_char_define` | `uuid, len, props, val, len, vh*, cccd*` | `int` | Define a characteristic |
| `ucx_gatt_server_activate` | - | `int` | Activate GATT server |
| `ucx_gatt_server_set_value` | `attr, value, len` | `int` | Set attribute value |
| `ucx_gatt_server_send_notification` | `conn, char, value, len` | `int` | Send notification |

### Web Serial API Flow

1. `SerialManager.connect()` calls `navigator.serial.requestPort()` (or `getPorts()` for auto-connect)
2. Port opens at selected baud rate (default 115200)
3. Background reader loop pushes chunks into `RingBuffer` (16 KB capacity)
4. `WasmBridge` wires `Module.serialWrite`/`serialRead`/`serialAvailable` to `SerialManager`
5. C code in `u_port_web.c` calls these via `EM_JS` functions
6. Write queue prevents `WritableStream` lock conflicts on concurrent writes
7. Disconnect listener cleans up on USB unplug

### ASYNCIFY Memory Gotcha

Emscripten ASYNCIFY rewrites the WASM binary to support async calls (e.g., `emscripten_sleep`, serial I/O).
When the WASM memory grows, **all JavaScript typed array views** (`HEAP8`, `HEAP32`, `HEAPU8`, `wasmMemory`) become detached/stale.

**Symptom**: Reading `HEAP32[ptr >> 2]` returns 0 after any async WASM call.

**Solution**: The `ucx_read_int32(int* ptr)` C helper reads the value inside WASM (where the pointer is always valid) and returns it to JavaScript via `ccall`. This avoids all typed-array staleness.

### Build System

- **CMake + Emscripten toolchain**: `emcmake cmake` configures, `emmake cmake --build` compiles
- **Windows link workaround**: The `.cmd` script calls `emcc.py` directly via Python to bypass `emcc.bat`/`emcc.ps1` wrapper bugs
- **ASYNCIFY enabled**: `-s ASYNCIFY=1 -s ASYNCIFY_IMPORTS=js_serial_write,js_serial_read`
- **26 C source files** compiled (ucxclient core + web wrappers)
- **Output**: `ucxclient.js` (~23 KB) + `ucxclient.wasm` (~72 KB)

## Troubleshooting

### Web Serial API Not Available
- **Solution**: Use Chrome or Edge 89+. Ensure HTTPS or localhost.

### Serial Port Connection Fails
- Verify NORA-W36 connected via USB (check Device Manager)
- Close other applications using the serial port
- Try different baud rate (115200 is default for NORA-W36)

### WiFi Scan Returns No Results
- Wait 5-10 seconds (active scan takes time)
- Check console log for TX/RX traffic
- Verify NORA-W36 firmware supports WiFi scanning (v3.2.0+)

### WiFi Scan Shows RSSI/Channel as 0
- Ensure the WASM binary includes `ucx_read_int32` (rebuild with `launch-ucx-web-app.cmd`)
- This was caused by ASYNCIFY heap-view staleness; the fix is built into `wasm-bridge.js`

### WASM Module Not Loading
- Must serve via HTTP (not `file://` URLs)
- Verify `ucxclient.js` and `ucxclient.wasm` exist in `ucx-web-app/`
- Rebuild: run `launch-ucx-web-app.cmd`

## Browser Compatibility

| Browser | Web Serial API | Status |
|---------|----------------|--------|
| Chrome 89+ | Yes | Tested and working |
| Edge 89+ | Yes | Tested and working |
| Opera 75+ | Yes | Should work (not tested) |
| Safari | No | Not supported |
| Firefox | No | Not supported |

## Testing

Tested with real NORA-W36 hardware on Windows 10/11, Chrome 131+:

- Serial connection at 115200 baud (auto-connect and manual)
- WiFi scanning with correct SSID, RSSI (dBm), and channel values
- WiFi connect/disconnect with WPA2 credentials
- IP address retrieval via DHCP
- BLE scanning with device names, addresses, and RSSI
- BLE connect/disconnect to peripherals
- GATT client service and characteristic discovery
- GATT client read/write and notification subscription
- GATT server service/characteristic definition and activation
- URC event handling (WiFi: +UEWLU, +UEWSNU, +UEWSND; BLE: +UEBTC, +UEBTDC, +UEBTGCN, +UEBTGSW)
- Serial disconnect detection and cleanup
- RX line reassembly from fragmented serial chunks

## Development

### Adding New Features

1. **Add C wrapper** in `ucx_wasm_wrapper.c`:
   ```c
   EMSCRIPTEN_KEEPALIVE
   int ucx_my_feature(int param) {
       return uCxMyFeature(&g_instance->cx_handle, param);
   }
   ```

2. **Export the function** in the `EXPORTED_FUNCTIONS` list in `launch-ucx-web-app.cmd`

3. **Add JS wrapper** in `wasm-bridge.js`:
   ```javascript
   async myFeature(param) {
       return await this.module.ccall(
           'ucx_my_feature', 'number', ['number'], [param]);
   }
   ```

4. **Wire UI** in `app.js` and `index.html`

### Debugging

Set log level from the browser console:
```javascript
bridge.setLogLevel(3); // 0=none, 1=error, 2=info, 3=debug
```

## License & Credits

**Copyright u-blox**

**Components**:
- **ucxclient**: u-blox UCX API C library
- **Emscripten**: LLVM-based WebAssembly compiler (MIT License)
- **Web Serial API**: W3C Community standard

**Contributors**:
- Original Windows app: u-blox team
- WebAssembly port: Christian Magnusson (u-blox)

---

# Disclaimer
Copyright &#x00a9; u-blox

u-blox reserves all rights in this deliverable (documentation, software, etc.,
hereafter "Deliverable").

u-blox grants you the right to use, copy, modify and distribute the
Deliverable provided hereunder for any purpose without fee.

THIS DELIVERABLE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR IMPLIED
WARRANTY. IN PARTICULAR, NEITHER THE AUTHOR NOR U-BLOX MAKES ANY
REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE MERCHANTABILITY OF THIS
DELIVERABLE OR ITS FITNESS FOR ANY PARTICULAR PURPOSE.

In case you provide us a feedback or make a contribution in the form of a
further development of the Deliverable ("Contribution"), u-blox will have the
same rights as granted to you, namely to use, copy, modify and distribute the
Contribution provided to us for any purpose without fee.
