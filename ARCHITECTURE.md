# UCX Avalonia App - Architecture Summary

**Date:** December 8, 2025  
**Status:** ✅ 100% Auto-Generated API Wrapper

---

## 🎯 Design Philosophy

**Use 100% auto-generated UCX API wrapper - NO manual function-by-function wrappers!**

All 365+ UCX API functions are auto-generated and directly accessible from C#.

---

## 📐 Architecture Layers

```
┌─────────────────────────────────────────────────────────────┐
│  C# Application (Avalonia UI)                               │
│  - MainWindowViewModel.cs                                   │
│  - UcxClient.cs (high-level managed wrapper)                │
└──────────────────────┬──────────────────────────────────────┘
                       │ P/Invoke
┌──────────────────────▼──────────────────────────────────────┐
│  Auto-Generated API (365+ functions)                        │
│  📄 UcxNativeGenerated.cs - C# P/Invoke declarations        │
│  ├─ ucx_wifi_StationConnect()                               │
│  ├─ ucx_wifi_StationScan1Begin/GetNext()                    │
│  ├─ ucx_bluetooth_Connect()                                 │
│  ├─ ucx_http_Request()                                      │
│  └─ ... 361 more functions                                  │
└──────────────────────┬──────────────────────────────────────┘
                       │ DLL
┌──────────────────────▼──────────────────────────────────────┐
│  Native Wrapper DLL (ucxclient_wrapper.dll)                 │
│  📄 ucxclient_wrapper_generated.c - C implementation        │
│  - Auto-generated wrappers that call ucxclient library      │
│  - Handles instance → handle translation                    │
└──────────────────────┬──────────────────────────────────────┘
                       │
┌──────────────────────▼──────────────────────────────────────┐
│  Core Functions (6 manual functions ONLY)                   │
│  📄 ucxclient_wrapper_core.c                                │
│  ✅ ucx_create() - Instance creation & UART open            │
│  ✅ ucx_destroy() - Cleanup                                 │
│  ✅ ucx_set_urc_callback() - URC forwarding to C#           │
│  ✅ ucx_set_log_callback() - Log forwarding to C#           │
│  ✅ ucx_get_last_error() - Error messages                   │
│  ✅ ucx_End() - End Begin/GetNext sequences                 │
└──────────────────────┬──────────────────────────────────────┘
                       │
┌──────────────────────▼──────────────────────────────────────┐
│  ucxclient Library                                          │
│  - AT command client                                        │
│  - URC handling                                             │
│  - UART abstraction                                         │
│  - UCX protocol implementation                              │
└─────────────────────────────────────────────────────────────┘
```

---

## 🔧 Core Components

### 1. **Code Generator** (`generate_wrapper.py`)

**Purpose:** Auto-generate C wrapper and C# P/Invoke for all UCX API functions

**Input:**
- `ucxclient/ucx_api/generated/NORA-W36X/*.h` (15 header files)

**Output:**
- `ucxclient_wrapper_generated.h` - C header (365 function declarations)
- `ucxclient_wrapper_generated.c` - C implementation (calls ucxclient)
- `UcxNativeGenerated.cs` - C# P/Invoke declarations

**Modules Covered:**
- Bluetooth (ucx_bluetooth_*)
- WiFi (ucx_wifi_*)
- HTTP (ucx_http_*)
- MQTT (ucx_mqtt_*)
- Socket (ucx_socket_*)
- Security (ucx_security_*)
- System (ucx_system_*)
- SPS (ucx_sps_*)
- GATT Client/Server (ucx_gatt_*)
- Diagnostics, Network Time, Power, General

**Total Functions:** 365+

---

### 2. **Core Wrapper** (`ucxclient_wrapper_core.c`)

**Manual Functions (6 ONLY):**

| Function | Purpose | Why Manual? |
|----------|---------|-------------|
| `ucx_create()` | Initialize instance, open UART, register URC handlers | Complex initialization logic |
| `ucx_destroy()` | Close UART, cleanup | Resource management |
| `ucx_set_urc_callback()` | Forward URCs to C# | Callback bridging |
| `ucx_set_log_callback()` | Forward logs to C# | Callback bridging |
| `ucx_get_last_error()` | Get error messages | Error handling |
| `ucx_End()` | End Begin/GetNext sequences | Helper for iterator patterns |

**❌ Removed Manual Functions:**
- ~~`ucx_wifi_scan()`~~ → Use `ucx_wifi_StationScan1Begin/GetNext()` directly
- ~~`ucx_wifi_connect()`~~ → Use `ucx_wifi_StationConnect()` directly
- ~~`ucx_wifi_disconnect()`~~ → Use `ucx_wifi_StationDisconnect()` directly  
- ~~`ucx_wifi_get_connection_info()`~~ → Use `ucx_wifi_StationGetNetworkStatus()` directly
- ~~`ucx_bluetooth_connect()`~~ → Use `ucx_bluetooth_Connect()` directly

---

### 3. **C# Managed Wrapper** (`UcxClient.cs`)

**High-Level Managed API:**

```csharp
public class UcxClient
{
    // Core lifecycle
    public UcxClient(string portName, int baudRate)
    public void Dispose()
    
    // WiFi - using generated API directly
    public async Task<List<WifiScanResult>> ScanWifiAsync()
    {
        UcxNative.ucx_wifi_StationScan1Begin_Direct(_handle, 0);
        while (UcxNative.ucx_wifi_StationScan1GetNext_Direct(_handle, out result)) {
            // Collect results
        }
        UcxNative.ucx_End(_handle);
    }
    
    public async Task ConnectWifiAsync(string ssid, string password)
    {
        UcxNativeGenerated.ucx_wifi_StationSetConnectionParams(...);
        UcxNativeGenerated.ucx_wifi_StationSetSecurityWpa(...);
        UcxNativeGenerated.ucx_wifi_StationConnect(...);
    }
    
    public async Task DisconnectWifiAsync()
    {
        UcxNativeGenerated.ucx_wifi_StationDisconnect(...);
    }
}
```

**Pattern:** Managed wrapper provides async/await, error handling, and convenience methods while calling generated API directly.

---

## 📋 How to Add New Functionality

### ✅ **Correct Approach (100% Generated):**

1. **Identify UCX API function** in `ucxclient/ucx_api/generated/NORA-W36X/*.h`
2. **Use generated function directly** from C#:
   ```csharp
   int result = UcxNativeGenerated.ucx_<module>_<Function>(_handle, ...);
   ```
3. **Add high-level wrapper** in `UcxClient.cs` if needed (optional for convenience)

### ❌ **Wrong Approach (Manual Wrapper):**

~~Don't create manual wrapper functions in `ucxclient_wrapper_core.c`!~~

---

## 🔄 Begin/GetNext Pattern

Many UCX API functions use an iterator pattern:

**Example: WiFi Scan**
```csharp
// Start iteration
UcxNativeGenerated.ucx_wifi_StationScan1Begin(_handle, scan_mode);

// Get results one by one
UcxNative.UcxWifiStationScan result;
while (UcxNative.ucx_wifi_StationScan1GetNext_Direct(_handle, out result))
{
    // Process result
    string ssid = Marshal.PtrToStringAnsi(result.ssid);
    int rssi = result.rssi;
}

// IMPORTANT: Always call End!
UcxNative.ucx_End(_handle);
```

**Other Begin/GetNext Functions:**
- `ucx_bluetooth_DiscoveryBegin/GetNext`
- `ucx_wifi_StationListNetworkStatusBegin/GetNext`
- `ucx_wifi_ApListConnectionsBegin/GetNext`
- And many more...

---

## 📦 Struct Marshalling

**Current Structs Defined in C#:**

```csharp
// For WiFi scanning
[StructLayout(LayoutKind.Sequential)]
public struct UcxWifiStationScan
{
    public UcxMacAddress bssid;
    public IntPtr ssid;  // const char*
    public int channel;
    public int rssi;
    public int authentication_suites;
    public int unicast_ciphers;
    public int group_ciphers;
}
```

**TODO - Need Struct Definitions:**
- `uSockIpAddress_t` - For IP addresses, subnet masks, gateways
- `uBtLeAddress_t` - For Bluetooth addresses
- `uCxWifiStationGetSecurity_t` - Security configuration
- Many more...

**How to Add:**
1. Find struct definition in `ucxclient/ucx_api/generated/NORA-W36X/*.h`
2. Create matching C# struct with `[StructLayout]` in `UcxNative.cs`
3. Use in P/Invoke declarations

---

## 🎨 File Organization

```
ucx-avalonia-app/
├── ucx-avalonia-app/              # C# Avalonia application
│   ├── Services/
│   │   ├── UcxClient.cs           # High-level managed wrapper
│   │   ├── UcxNative.cs           # Core 6 functions + structs
│   │   └── UcxNativeGenerated.cs  # ⚙️ AUTO-GENERATED (365+ functions)
│   ├── ViewModels/
│   └── Views/
│
├── ucxclient-wrapper/             # Native C wrapper DLL
│   ├── ucxclient_wrapper_core.c        # Core 6 functions (MANUAL)
│   ├── ucxclient_wrapper_generated.c   # ⚙️ AUTO-GENERATED
│   ├── ucxclient_wrapper_generated.h   # ⚙️ AUTO-GENERATED
│   ├── ucxclient_wrapper.h             # Public header
│   ├── generate_wrapper.py             # Code generator
│   └── CMakeLists.txt
│
└── ucxclient/                     # Git submodule (ucxclient library)
    └── ucx_api/generated/NORA-W36X/
        ├── u_cx_wifi.h            # WiFi API definitions
        ├── u_cx_bluetooth.h       # Bluetooth API definitions
        └── ... (13 more modules)
```

---

## 🚀 Running the Code Generator

```powershell
cd ucxclient-wrapper
python generate_wrapper.py
```

**Output:**
```
============================================================
UCX API Wrapper Code Generator
============================================================
Found 365 functions
Found 45 URC callbacks

Generating C wrapper...
  Generated: ucxclient_wrapper_generated.h
  Generated: ucxclient_wrapper_generated.c

Generating C# P/Invoke declarations...
  Generated: ../ucx-avalonia-app/Services/UcxNativeGenerated.cs

============================================================
Code generation complete!
============================================================
```

**When to regenerate:**
- ucxclient submodule updated
- New UCX API functions added
- Bug fixes in generator

---

## ✅ Current Status

| Feature | Status | Implementation |
|---------|--------|----------------|
| **Core Functions** | ✅ Complete | 6 manual functions |
| **Generated API** | ✅ Complete | 365+ auto-generated |
| **WiFi Scan** | ✅ Working | Direct generated API |
| **WiFi Connect** | ✅ Working | Direct generated API |
| **URC Handling** | ✅ Working | Network up/down events |
| **Bluetooth** | ⏳ Generated | Not yet tested |
| **HTTP** | ⏳ Generated | Not yet tested |
| **MQTT** | ⏳ Generated | Not yet tested |
| **Struct Marshalling** | ⚠️ Partial | More structs needed |

---

## 🎓 Key Lessons Learned

1. ✅ **Don't create manual wrappers** - Use generated API directly
2. ✅ **Keep core minimal** - Only 6 instance management functions
3. ✅ **Structs need care** - Proper `[StructLayout]` marshalling required
4. ✅ **Begin/GetNext pattern** - Always call `ucx_End()` after iteration
5. ✅ **Code generation scales** - 365 functions with minimal effort

---

## 📚 References

- **Code Generator Docs:** `ucxclient-wrapper/CODEGEN_README.md`
- **UCX API Reference:** `ucxclient/ucx_api/generated/NORA-W36X/*.h`
- **Avalonia Docs:** https://docs.avaloniaui.net/
- **P/Invoke Guide:** https://learn.microsoft.com/dotnet/standard/native-interop/pinvoke

---

**Last Updated:** December 8, 2025  
**Maintainer:** u-blox  
**License:** Apache 2.0
