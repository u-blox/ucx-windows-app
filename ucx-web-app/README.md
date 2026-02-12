# UCX Web App - Implementation Details

Detailed implementation documentation for the browser-based NORA-W36 WebAssembly application.

For a project overview, build instructions, and quick start guide, see the [root README](../README.md).

## File Reference

| File | Lines | Purpose |
|------|-------|---------|
| `index.html` | ~200 | Markup + CSS (loads JS modules below) |
| `serial.js` | ~230 | `SerialManager` class + `RingBuffer` for Web Serial I/O |
| `wasm-bridge.js` | ~470 | `WasmBridge` class wrapping the Emscripten module |
| `app.js` | ~600 | UI logic, URC handling, RX line reassembly |
| `library.js` | ~30 | Empty `mergeInto` stub (kept for `--js-library` flag) |
| `ucx_wasm_wrapper.c` | ~910 | WASM-exported C functions (33 exports) |
| `u_port_web.c` | ~370 | Platform port: EM_JS serial bridge + timer |
| `u_port_clib.h` | ~50 | Mutex/thread no-op macros for single-threaded WASM |
| `CMakeLists.txt` | ~80 | Emscripten build configuration |

## JavaScript Modules

### serial.js - SerialManager

Manages the Web Serial API connection lifecycle.

**Classes**:
- `RingBuffer(capacity=16384)` - Circular byte buffer with `push()`, `read()`, `available`, `clear()`
- `SerialManager` - Connection management:
  - `autoConnect(baudRate)` - Reconnect to previously paired port
  - `connect(baudRate)` - Prompt user to select a port
  - `disconnect()` - Close port and clean up
  - `serialWrite(buffer)` - Queued writes (prevents `WritableStream` lock conflicts)
  - `serialRead(maxLength)` - Read from ring buffer (called by WASM)
  - `serialAvailable()` - Bytes waiting in ring buffer
  - Callbacks: `onReceive(text, bytes)`, `onDisconnect()`, `onError(msg)`

### wasm-bridge.js - WasmBridge

Loads the Emscripten WASM module and provides a typed async JavaScript API.

**Constructor**: `new WasmBridge(serialManager)`

**Methods**:
- `load()` - Load WASM module and wire serial I/O hooks
- `init(baudRate)` - Initialize UCX AT client (returns 0 on success)
- `deinit()` - Clean up UCX client
- `setLogLevel(level)` - 0=none, 1=error, 2=info, 3=debug
- `wifiScan()` - Returns `[{ssid, rssi, channel}, ...]`
- `wifiConnect(ssid, password)` - Returns 0 on success
- `wifiDisconnect()` - Returns 0 on success
- `getIP()` - Returns IP string or null
- `getVersion()` - Returns firmware version string or null
- `btScan(timeoutMs)` - Returns `[{addr, rssi, name}, ...]`
- `btConnect(addrStr)` - Returns connection handle
- `btDisconnect(connHandle)` - Returns 0 on success
- `gattDiscoverServices(connHandle)` - Returns `[{startHandle, endHandle, uuid}, ...]`
- `gattDiscoverChars(conn, start, end)` - Returns `[{attrHandle, valueHandle, properties, uuid}, ...]`
- `gattRead(connHandle, valueHandle)` - Returns `Uint8Array`
- `gattWrite(connHandle, valueHandle, data)` - Returns 0 on success
- `gattConfigWrite(conn, cccd, config)` - Enable notifications/indications
- `gattServerDefineService(uuidBytes)` - Returns service handle
- `gattServerDefineChar(uuid, props, initialValue)` - Returns `{valueHandle, cccdHandle}`
- `gattServerActivate()` - Returns 0 on success
- `gattServerSetValue(attrHandle, value)` - Returns 0 on success
- `gattServerNotify(conn, charHandle, value)` - Returns 0 on success
- `btAdvertiseStart()` / `btAdvertiseStop()` - Returns 0 on success
- `_readInt32(ptr)` - Calls `ucx_read_int32` via ccall (safe ASYNCIFY memory read)

### app.js - Application Logic

Top-level UI orchestration, wired to `index.html` buttons.

**Key functions**:
- `initApp()` - Creates `SerialManager` + `WasmBridge`, loads WASM, attempts auto-connect
- `connectSerial()` / `disconnectSerial()` / `autoConnectSerial()` - Serial lifecycle
- `wifiScan()` / `wifiConnect()` / `wifiDisconnect()` / `getConnectionInfo()` - WiFi ops
- `btScan()` / `btConnect()` / `btDisconnect()` - BLE scan and connection management
- `gattDiscover()` / `gattDiscoverChars()` - GATT service/char discovery with inline actions
- `gattReadChar()` / `promptGattWrite()` / `gattEnableNotify()` - GATT client operations
- `gattServerDefine()` / `gattServerActivate()` - GATT server setup
- `gattServerSetValue()` / `gattServerNotify()` - GATT server data operations
- `btAdvertiseStart()` / `btAdvertiseStop()` - BLE advertising control
- `handleURC(urc)` - Dispatches WiFi (+UEWLU, +UEWSNU, +UEWSND) and BLE (+UEBTC, +UEBTDC, +UEBTGCN, +UEBTGSW) events
- `onSerialReceive(text)` - RX line reassembly buffer (displays complete lines only)
- `log(message)` / `clearLog()` - Timestamped UI console

## C Source Files

### ucx_wasm_wrapper.c

Simplified C API exported to JavaScript. Wraps the full ucxclient library.

**Log level macros**: `LOG_ERROR`, `LOG_INFO`, `LOG_DEBUG` (controlled by `g_log_level`)

**Instance**: Single `ucx_wasm_instance_t` containing AT client, CX handle, RX/URC buffers.

**Exported functions** (see root README for full table):
- `ucx_init`, `ucx_deinit` - Lifecycle
- `ucx_set_log_level`, `ucx_read_int32` - Utilities
- `ucx_wifi_scan_begin/get_next/end` - WiFi scanning
- `ucx_wifi_connect`, `ucx_wifi_disconnect`, `ucx_wifi_get_ip` - WiFi management
- `ucx_bt_discovery_begin/get_next/end` - BLE scanning
- `ucx_bt_connect`, `ucx_bt_disconnect` - BLE connection management
- `ucx_bt_advertise_start/stop` - BLE advertising
- `ucx_gatt_discover_services_begin/get_next/end` - GATT service discovery
- `ucx_gatt_discover_chars_begin/get_next/end` - GATT characteristic discovery
- `ucx_gatt_read`, `ucx_gatt_write`, `ucx_gatt_config_write` - GATT client operations
- `ucx_gatt_server_service_define`, `ucx_gatt_server_char_define` - GATT server setup
- `ucx_gatt_server_activate`, `ucx_gatt_server_set_value`, `ucx_gatt_server_send_notification` - GATT server operations
- `ucx_send_at_command`, `ucx_get_version`, `ucx_get_last_error` - System

### u_port_web.c

Platform port layer bridging ucxclient's UART abstraction to Web Serial API.

**EM_JS functions** (called from C, executed in JavaScript):
- `js_serial_write(data, length)` - Copies bytes to `Uint8Array`, calls `Module.serialWrite()`
- `js_serial_read(buffer, maxLength)` - Calls `Module.serialRead()`, copies back to WASM heap
- `js_get_time_ms()` - Returns `Date.now() & 0x7FFFFFFF`

**Port API implementations**:
- `uPortUartOpen()` / `uPortUartClose()` - Open/close (no-op; port managed by JS)
- `uPortUartRead()` / `uPortUartWrite()` - Delegate to EM_JS functions
- `uPortGetTickTimeMs()` - Millisecond timer
- Mutex functions - All no-ops (single-threaded WASM)
- `uPortLog()` - `printf` with configurable port log level (`PLOG_*` macros)

## Build Configuration

### CMakeLists.txt

- Project: `ucx-web-terminal` (CMake project name)
- Target: `ucxclient` → outputs `ucxclient.js` + `ucxclient.wasm`
- Module: `NORA-W36X` (sets `UCX_MODULE` for ucxclient.cmake)
- Memory: 16 MB initial, growable (`ALLOW_MEMORY_GROWTH`)
- Optimization: `-O2` (link), `-O3` in the `.cmd` script

### Link flags (from launch-ucx-web-app.cmd)

```
-s WASM=1
-s MODULARIZE=1
-s EXPORT_NAME=createUCXModule
-s EXPORTED_RUNTIME_METHODS=ccall,cwrap,UTF8ToString,stringToUTF8,lengthBytesUTF8
-s EXPORTED_FUNCTIONS=_ucx_init,_ucx_deinit,_ucx_set_log_level,_ucx_read_int32,
    _ucx_wifi_connect,_ucx_wifi_disconnect,_ucx_wifi_scan_begin,
    _ucx_wifi_scan_get_next,_ucx_wifi_scan_end,_ucx_wifi_get_ip,
    _ucx_send_at_command,_ucx_get_version,
    _ucx_bt_discovery_begin,_ucx_bt_discovery_get_next,_ucx_bt_discovery_end,
    _ucx_bt_connect,_ucx_bt_disconnect,
    _ucx_bt_advertise_start,_ucx_bt_advertise_stop,
    _ucx_gatt_discover_services_begin,_ucx_gatt_discover_services_get_next,
    _ucx_gatt_discover_services_end,_ucx_gatt_discover_chars_begin,
    _ucx_gatt_discover_chars_get_next,_ucx_gatt_discover_chars_end,
    _ucx_gatt_read,_ucx_gatt_write,_ucx_gatt_config_write,
    _ucx_gatt_server_service_define,_ucx_gatt_server_char_define,
    _ucx_gatt_server_activate,_ucx_gatt_server_set_value,
    _ucx_gatt_server_send_notification,_malloc,_free
-s ALLOW_MEMORY_GROWTH=1
-s INITIAL_MEMORY=16777216
-s ASYNCIFY=1
-s ASYNCIFY_IMPORTS=js_serial_write,js_serial_read
--js-library ../library.js
```

## Key Design Decisions

### Why EM_JS instead of library.js?

The original `library.js` contained `mergeInto(LibraryManager.library, {...})` functions.
These were moved to `EM_JS` blocks within `u_port_web.c` because:
- EM_JS functions can use ASYNCIFY (library.js functions cannot easily)
- The serial I/O hooks (`Module.serialWrite`, etc.) are wired at runtime by `WasmBridge`
- `library.js` is kept as an empty stub so the `--js-library` flag in the build command remains valid

### Why ucx_read_int32?

ASYNCIFY-instrumented WASM functions can grow memory during any async yield (`emscripten_sleep`, serial read/write). When memory grows, the `ArrayBuffer` backing `HEAP8`, `HEAP32`, etc. is detached. Any JavaScript code that cached a typed array view before the growth will read stale zeros.

`ucx_read_int32(int* ptr)` is a trivial C function that dereferences a pointer—inside WASM, where the linear memory is always valid regardless of `ArrayBuffer` detachment. JavaScript calls it via `ccall` to safely read `int32` values from WASM memory.

### Why RX line reassembly?

Serial data arrives in arbitrary-sized chunks (e.g., 64-byte USB packets). A single AT response line like `+UWSSC:0,"MyNetwork",-65,6` may span multiple chunks. Without reassembly, the console shows garbled partial lines. The `rxLineBuffer` in `app.js` accumulates text and only displays complete `\n`-delimited lines.

## Browser Console API

For debugging, the `serial` and `bridge` objects are global:

```javascript
// Set verbose logging
bridge.setLogLevel(3);

// Manual WiFi scan
const results = await bridge.wifiScan();
console.table(results);

// BLE scan
const devices = await bridge.btScan(5000);
console.table(devices);

// Connect to a BLE device
const handle = await bridge.btConnect('AABBCCDDEEFF');

// Discover GATT services
const services = await bridge.gattDiscoverServices(handle);
console.table(services);

// Get firmware version
const ver = await bridge.getVersion();
console.log(ver);

// Check serial buffer
console.log('RX bytes:', serial.serialAvailable());
```
