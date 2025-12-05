# UCX API Wrapper Code Generator

## Overview

This directory contains a **Python code generation script** (`generate_wrapper.py`) that automatically creates C wrapper functions and C# P/Invoke declarations for the entire UCX API.

### Why Code Generation?

The UCX API contains **365+ functions** across 15 modules (WiFi, Bluetooth, HTTP, MQTT, Socket, etc.) plus **45+ URC callbacks**. Manually creating wrappers would be:
- **Time-consuming**: Weeks of tedious work
- **Error-prone**: Easy to make mistakes in parameter marshaling
- **Unmaintainable**: UCX API updates require manual sync

### What It Generates

The script parses `ucxclient/ucx_api/generated/NORA-W36X/*.h` files and produces:

1. **`ucxclient_wrapper_generated.h`** - C header with wrapper function declarations
2. **`ucxclient_wrapper_generated.c`** - C implementation that calls UCX API functions
3. **`UcxNativeGenerated.cs`** - C# P/Invoke declarations for .NET interop

## Usage

### Running the Generator

```powershell
cd ucxclient-wrapper
python generate_wrapper.py
```

### Output

```
============================================================
UCX API Wrapper Code Generator
============================================================
UCX API directory: C:\u-blox\ucx-windows-app\ucxclient\ucx_api\generated\NORA-W36X

Parsing u_cx_bluetooth.h (bluetooth)...
Parsing u_cx_wifi.h (wifi)...
...

Found 365 functions
Found 45 URC callbacks

Generating C wrapper...
  Generated: ucxclient_wrapper_generated.h
  Generated: ucxclient_wrapper_generated.c

Generating C# P/Invoke declarations...
  Generated: UcxNativeGenerated.cs

============================================================
Code generation complete!
============================================================
```

## Current Status

### ✅ What Works

- ✅ **Header Parsing**: Successfully extracts all 365 functions from 15 UCX API modules
- ✅ **Function Discovery**: Identifies function names, return types, and parameters
- ✅ **Module Organization**: Groups functions by module (bluetooth, wifi, http, etc.)
- ✅ **URC Detection**: Finds all 45 URC callback registration functions
- ✅ **Basic Code Generation**: Creates compilable C/C# wrapper skeletons
- ✅ **Instance Management**: Automatically injects `ucx_instance_t*` parameter
- ✅ **Handle Translation**: Converts `inst` → `&inst->cx_handle` in function calls

### ⚠️ Known Issues (Needs Refinement)

1. **Pointer Parameter Handling**
   - Issue: Some output pointers get double/triple `**` or `***`
   - Example: `uBtMode_t * * pMode` (should be `uBtMode_t *pMode`)
   - Cause: Regex captures `*` as part of type name
   - Fix Needed: Better tokenization of pointer declarations

2. **String Parameter Marshaling**
   - Issue: C# generates `ref byte` for strings (should be `string` or `StringBuilder`)
   - Example: `ref byte device_name` (should be `string device_name`)
   - Cause: Pointer detection doesn't distinguish `char*` from `uint8_t*`
   - Fix Needed: Special case handling for `const char*` → `string`

3. **Struct Marshaling**
   - Issue: Complex structs not yet supported
   - Example: `uCxWifiStationGetSecurity_t` (union types)
   - Cause: C# struct definitions not generated
   - Fix Needed: Parse typedef structs and generate [StructLayout] declarations

4. **Array Parameters**
   - Issue: Byte arrays need special marshaling
   - Example: `const uint8_t *data, int data_len`
   - Cause: C# needs `byte[]` with `[MarshalAs]` attributes
   - Fix Needed: Detect array+length pairs and generate correct marshaling

5. **Begin/GetNext Patterns**
   - Issue: Iterator functions need special handling
   - Example: `WifiScanBegin()` + `WifiScanGetNext()`
   - Cause: Need wrapper to collect all results
   - Fix Needed: Generate higher-level "GetAll" wrappers

6. **URC Callback Wrappers**
   - Issue: URC callbacks not yet wrapped
   - Current: Manual implementation (like `wifi_network_up_urc`)
   - Needed: Auto-generate all 45 URC callbacks with generic forwarding

## Architecture

### Generated Function Pattern

**Input (UCX API):**
```c
int32_t uCxWifiStationConnect(uCxHandle_t *puCxHandle, int32_t connection_id);
```

**Output (C Wrapper):**
```c
int32_t ucx_wifi_StationConnect(ucx_instance_t *inst, int32_t connection_id)
{
    if (!inst) {
        ucx_wrapper_printf("[ERROR] NULL instance in %s\n", __func__);
        return -1;
    }
    
    int32_t result = uCxWifiStationConnect(&inst->cx_handle, connection_id);
    return result;
}
```

**Output (C# P/Invoke):**
```csharp
[DllImport("ucxclient_wrapper", CallingConvention = CallingConvention.Cdecl)]
public static extern int ucx_wifi_StationConnect(IntPtr instance, int connection_id);
```

### Naming Convention

- **UCX API**: `uCx<Module><Function>` (e.g., `uCxWifiStationConnect`)
- **C Wrapper**: `ucx_<module>_<Function>` (e.g., `ucx_wifi_StationConnect`)
- **C# Wrapper**: Same as C wrapper (e.g., `ucx_wifi_StationConnect`)

## Next Steps

### High Priority Fixes

1. **Fix Pointer Parsing**
   ```python
   # Current: Naive split on whitespace
   tokens = part.split()
   
   # Needed: Proper C type parser
   # Handle: "const char *", "uint8_t * const *", etc.
   ```

2. **Add Struct Generation**
   ```python
   # Parse typedef structs from u_cx_types.h
   # Generate C# [StructLayout] declarations
   # Handle unions, enums, nested structs
   ```

3. **Improve String Marshaling**
   ```python
   # Detect "const char *" → C# string
   # Detect "char *" (output) → C# StringBuilder
   # Detect "uint8_t *" → C# byte[]
   ```

### Medium Priority

4. **Generate URC Callbacks**
   ```python
   # For each uCx<Module>Register<Urc> function:
   # - Generate C callback function
   # - Forward to generic URC callback
   # - Auto-register in ucx_create()
   ```

5. **Create Iterator Wrappers**
   ```python
   # For Begin/GetNext patterns:
   # - Generate "GetAll" wrapper
   # - Return array/list of results
   # - Handle memory management
   ```

### Low Priority

6. **Add Error Handling**
   - Map UCX error codes to C# exceptions
   - Generate `UcxException` class

7. **Documentation Generation**
   - Extract comments from headers
   - Generate XML docs for C#
   - Create API reference markdown

## Manual Wrappers (Keep These)

Some functions are too complex for auto-generation and should remain manual:

- **`ucx_create`** - Complex initialization with UART config
- **`ucx_destroy`** - Resource cleanup
- **`ucx_wifi_scan`** - Custom result parsing (manual WiFi network iteration)
- **`ucx_wifi_connect`** - Multi-step sequence (security + params + connect)
- **`ucx_wifi_get_connection_info`** - Custom struct population

These are in `ucxclient_wrapper.c` and should **not** be overwritten by generated code.

## Integration with Build System

### Option 1: Pre-Build Step (Recommended)

Add to `CMakeLists.txt`:
```cmake
find_package(Python3 COMPONENTS Interpreter REQUIRED)

add_custom_command(
    OUTPUT ucxclient_wrapper_generated.c ucxclient_wrapper_generated.h
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/generate_wrapper.py
    DEPENDS generate_wrapper.py
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Generating UCX API wrappers..."
)

add_library(ucxclient_wrapper SHARED
    ucxclient_wrapper.c
    ucxclient_wrapper_generated.c  # Auto-generated
    ...
)
```

### Option 2: Manual Generation

Run script manually when UCX API changes:
```powershell
cd ucxclient-wrapper
python generate_wrapper.py
```

Then commit generated files to version control.

## Testing

### Verify Generation

```powershell
# Generate code
python generate_wrapper.py

# Count generated functions
Select-String "^int32_t ucx_" ucxclient_wrapper_generated.h | Measure-Object

# Expected: ~365 function declarations
```

### Compile Test

```powershell
cd ..\build-wrapper
cmake ..
cmake --build . --config Release
```

### Runtime Test

Once pointer handling is fixed, the generated wrappers should work with existing code:
```csharp
// This should work once string marshaling is fixed:
var result = UcxNativeGenerated.ucx_bluetooth_SetLocalName(
    instancePtr,
    "MyDevice"  // Currently broken, needs string marshaling fix
);
```

## Examples of Generated Code

### Simple Value Function

**UCX API:**
```c
int32_t uCxBluetoothSetMode(uCxHandle_t *puCxHandle, uBtMode_t mode);
```

**Generated C:**
```c
int32_t ucx_bluetooth_SetMode(ucx_instance_t *inst, uBtMode_t mode)
{
    if (!inst) {
        ucx_wrapper_printf("[ERROR] NULL instance\n");
        return -1;
    }
    int32_t result = uCxBluetoothSetMode(&inst->cx_handle, mode);
    return result;
}
```

**Generated C#:**
```csharp
[DllImport("ucxclient_wrapper", CallingConvention = CallingConvention.Cdecl)]
public static extern int ucx_bluetooth_SetMode(IntPtr instance, uBtMode_t mode);
```

### Output Parameter Function

**UCX API:**
```c
int32_t uCxBluetoothGetRssi(uCxHandle_t *puCxHandle, int32_t conn_handle, int32_t *pRssi);
```

**Generated C:**
```c
int32_t ucx_bluetooth_GetRssi(ucx_instance_t *inst, int32_t conn_handle, int32_t *pRssi)
{
    if (!inst) return -1;
    int32_t result = uCxBluetoothGetRssi(&inst->cx_handle, conn_handle, pRssi);
    return result;
}
```

**Generated C# (needs fixing):**
```csharp
// Current (wrong):
public static extern int ucx_bluetooth_GetRssi(IntPtr instance, int conn_handle, ref int pRssi);

// Should be:
public static extern int ucx_bluetooth_GetRssi(IntPtr instance, int conn_handle, out int rssi);
```

## Contributing

To improve the code generator:

1. **Test with specific UCX API functions** - Find edge cases
2. **Improve type parsing** - Better regex or use a C parser library
3. **Add marshaling rules** - Document C ↔ C# type mappings
4. **Generate documentation** - Extract comments from headers

## License

Same as ucx-windows-app project.

## Contact

For questions about code generation, see:
- Main README: `../README.md`
- UCX API docs: `../ucxclient/README.md`
