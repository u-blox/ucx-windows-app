/*
 * Copyright 2025 u-blox
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file ucx_wasm_wrapper.c
 * @brief WebAssembly wrapper functions for UCX API
 * 
 * This file provides simplified C functions that can be easily called
 * from JavaScript. These wrap the full UCX API to make it more accessible
 * from the browser.
 */

#include <emscripten.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// UCX includes
#include "u_cx.h"
#include "u_cx_at_client.h"
#include "u_cx_wifi.h"
#include "u_cx_bluetooth.h"
#include "u_cx_gatt_client.h"
#include "u_cx_gatt_server.h"
#include "u_cx_system.h"
#include "u_cx_general.h"

/* ----------------------------------------------------------------
 * LOG LEVELS
 * -------------------------------------------------------------- */

#define UCX_LOG_NONE    0
#define UCX_LOG_ERROR   1
#define UCX_LOG_INFO    2
#define UCX_LOG_DEBUG   3

static int g_log_level = UCX_LOG_INFO;

#define LOG_ERROR(fmt, ...) do { if (g_log_level >= UCX_LOG_ERROR) printf("[WASM-ERR] " fmt "\n", ##__VA_ARGS__); } while(0)
#define LOG_INFO(fmt, ...)  do { if (g_log_level >= UCX_LOG_INFO)  printf("[WASM] " fmt "\n", ##__VA_ARGS__); } while(0)
#define LOG_DEBUG(fmt, ...) do { if (g_log_level >= UCX_LOG_DEBUG) printf("[WASM-DBG] " fmt "\n", ##__VA_ARGS__); } while(0)

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

#define RX_BUFFER_SIZE 2048
#define URC_BUFFER_SIZE 512

typedef struct {
    uCxAtClient_t at_client;
    uCxHandle_t cx_handle;
    uCxAtClientConfig_t config;
    uint8_t rx_buffer[RX_BUFFER_SIZE];
    uint8_t urc_buffer[URC_BUFFER_SIZE];
    char error_msg[256];
} ucx_wasm_instance_t;

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

static ucx_wasm_instance_t* g_instance = NULL;

/* ----------------------------------------------------------------
 * URC CALLBACK
 * -------------------------------------------------------------- */

/**
 * JavaScript URC callback (implemented in library.js)
 */
EM_JS(void, js_urc_callback, (const char* urc_line), {
    if (Module.onURC) {
        Module.onURC(UTF8ToString(urc_line));
    }
});

/**
 * Internal URC handler - forwards to JavaScript
 */
static void internal_urc_callback(struct uCxAtClient *pClient, void *pTag,
                                   char *pLine, size_t lineLength,
                                   uint8_t *pBinaryData, size_t binaryDataLen) {
    // Copy URC line (ensure null termination)
    char urc_line[256];
    size_t copy_len = lineLength < sizeof(urc_line) - 1 ? lineLength : sizeof(urc_line) - 1;
    memcpy(urc_line, pLine, copy_len);
    urc_line[copy_len] = '\0';
    
    LOG_DEBUG("URC: %s", urc_line);
    
    // Forward to JavaScript
    js_urc_callback(urc_line);
}

/* ----------------------------------------------------------------
 * CORE FUNCTIONS
 * -------------------------------------------------------------- */

/**
 * Initialize UCX client
 * @param portName Serial port name (usually "web_serial")
 * @param baudRate Baud rate (usually 115200)
 * @return 0 on success, -1 on error
 */
/**
 * Read an int32 value from a WASM memory pointer.
 * Used by JS to safely read memory without relying on HEAP32 views
 * which can become stale after ASYNCIFY memory growth.
 */
EMSCRIPTEN_KEEPALIVE
int ucx_read_int32(int* ptr) {
    return ptr ? *ptr : 0;
}

/**
 * Set log verbosity level.
 * @param level 0=none, 1=error, 2=info, 3=debug
 */
EMSCRIPTEN_KEEPALIVE
void ucx_set_log_level(int level) {
    if (level >= UCX_LOG_NONE && level <= UCX_LOG_DEBUG) {
        g_log_level = level;
    }
}

EMSCRIPTEN_KEEPALIVE
int ucx_init(const char* portName, int baudRate) {
    if (g_instance) {
        LOG_INFO("UCX already initialized");
        return 0;
    }
    
    LOG_INFO("Initializing UCX: port=%s, baud=%d", portName, baudRate);
    
    // Allocate instance
    g_instance = (ucx_wasm_instance_t*)malloc(sizeof(ucx_wasm_instance_t));
    if (!g_instance) {
        LOG_ERROR("Failed to allocate instance");
        return -1;
    }
    
    memset(g_instance, 0, sizeof(ucx_wasm_instance_t));
    
    // Configure AT client
    g_instance->config.pRxBuffer = g_instance->rx_buffer;
    g_instance->config.rxBufferLen = RX_BUFFER_SIZE;
    g_instance->config.pUrcBuffer = g_instance->urc_buffer;
    g_instance->config.urcBufferLen = URC_BUFFER_SIZE;
    g_instance->config.pUartDevName = portName;
    g_instance->config.timeoutMs = 20000;  // 20 seconds timeout for WiFi operations
    
    // Initialize AT client
    uCxAtClientInit(&g_instance->config, &g_instance->at_client);
    
    // Open UART (Web Serial port)
    int32_t result = uCxAtClientOpen(&g_instance->at_client, baudRate, false);
    if (result < 0) {
        LOG_ERROR("Failed to open UART: %d", result);
        free(g_instance);
        g_instance = NULL;
        return -1;
    }
    
    // Set URC callback
    uCxAtClientSetUrcCallback(&g_instance->at_client, internal_urc_callback, g_instance);
    
    // Initialize uCX handle
    uCxInit(&g_instance->at_client, &g_instance->cx_handle);
    
    // Turn off echo mode for better AT command parsing
    LOG_DEBUG("Disabling AT echo (ATE0)...");
    result = uCxSystemSetEchoOff(&g_instance->cx_handle);
    if (result != 0) {
        LOG_INFO("Warning: Failed to disable echo: %d (continuing anyway)", result);
    } else {
        LOG_DEBUG("AT echo disabled");
    }
    
    LOG_INFO("UCX initialized successfully");
    return 0;
}

/**
 * Deinitialize UCX client
 */
EMSCRIPTEN_KEEPALIVE
void ucx_deinit(void) {
    if (!g_instance) {
        return;
    }
    
    LOG_INFO("Deinitializing UCX");
    
    uCxAtClientClose(&g_instance->at_client);
    free(g_instance);
    g_instance = NULL;
}

/**
 * Get last error message
 * @return Error string, or NULL if no error
 */
EMSCRIPTEN_KEEPALIVE
const char* ucx_get_last_error(void) {
    if (!g_instance || g_instance->error_msg[0] == '\0') {
        return NULL;
    }
    return g_instance->error_msg;
}

/* ----------------------------------------------------------------
 * WIFI FUNCTIONS
 * -------------------------------------------------------------- */

/**
 * Start WiFi scan (passive mode)
 * @return 0 on success, negative on error
 */
EMSCRIPTEN_KEEPALIVE
int ucx_wifi_scan_begin(void) {
    if (!g_instance) {
        return -1;
    }
    
    LOG_INFO("Starting WiFi scan");
    uCxWifiStationScan1Begin(&g_instance->cx_handle, U_WIFI_SCAN_MODE_ACTIVE);
    
    return 0;
}

/**
 * Get next WiFi scan result
 * @param ssid Output buffer for SSID (min 33 bytes)
 * @param rssi Output pointer for RSSI value
 * @param channel Output pointer for channel number
 * @return 1 if result available, 0 if no more results, -1 on error
 */
EMSCRIPTEN_KEEPALIVE
int ucx_wifi_scan_get_next(char* ssid, int* rssi, int* channel) {
    if (!g_instance || !ssid || !rssi || !channel) {
        return -1;
    }
    
    uCxWifiStationScan_t scan_result;
    bool has_more = uCxWifiStationScan1GetNext(&g_instance->cx_handle, &scan_result);
    
    if (!has_more) {
        return 0; // No more results
    }
    
    // Copy scan result data
    strncpy(ssid, scan_result.ssid, 32);
    ssid[32] = '\0';
    *rssi = scan_result.rssi;
    *channel = scan_result.channel;
    
    LOG_DEBUG("Scan result: %s (ch:%d, rssi:%d)", ssid, *channel, *rssi);
    
    return 1; // Result available
}

/**
 * End WiFi scan (cleanup)
 */
EMSCRIPTEN_KEEPALIVE
void ucx_wifi_scan_end(void) {
    if (!g_instance) {
        return;
    }
    
    uCxEnd(&g_instance->cx_handle);
    LOG_INFO("WiFi scan ended");
}

/**
 * Connect to WiFi network
 * @param ssid Network SSID
 * @param password Network password (NULL or empty for open network)
 * @return 0 on success, negative on error
 */
EMSCRIPTEN_KEEPALIVE
int ucx_wifi_connect(const char* ssid, const char* password) {
    if (!g_instance) {
        LOG_ERROR("Not initialized");
        return -1;
    }
    if (!ssid) {
        LOG_ERROR("SSID is NULL");
        return -1;
    }
    
    LOG_INFO("WiFi connect: SSID=\"%s\", security=%s", ssid,
             (password && strlen(password) > 0) ? "WPA" : "OPEN");
    
    int32_t wlan_handle = 0; // Station interface
    int32_t result;
    
    // Step 1: Set connection parameters (SSID)
    LOG_DEBUG("Setting connection params (SSID)");
    result = uCxWifiStationSetConnectionParams(&g_instance->cx_handle, wlan_handle, ssid);
    if (result != 0) {
        LOG_ERROR("Failed to set SSID: %d", result);
        snprintf(g_instance->error_msg, sizeof(g_instance->error_msg),
                 "Failed to set SSID: %d", result);
        return result;
    }
    
    // Step 2: Set security
    if (password && strlen(password) > 0) {
        LOG_DEBUG("Setting WPA security");
        result = uCxWifiStationSetSecurityWpa(&g_instance->cx_handle, 
                                             wlan_handle, 
                                             password, 
                                             U_WIFI_WPA_THRESHOLD_WPA2);
        if (result != 0) {
            LOG_ERROR("Failed to set WPA security: %d", result);
            snprintf(g_instance->error_msg, sizeof(g_instance->error_msg),
                     "Failed to set WPA security: %d", result);
            return result;
        }
    } else {
        LOG_DEBUG("Setting open security");
        result = uCxWifiStationSetSecurityOpen(&g_instance->cx_handle, wlan_handle);
        if (result != 0) {
            LOG_ERROR("Failed to set open security: %d", result);
            snprintf(g_instance->error_msg, sizeof(g_instance->error_msg),
                     "Failed to set open security: %d", result);
            return result;
        }
    }
    
    // Step 3: Connect
    LOG_DEBUG("Sending connect command");
    result = uCxWifiStationConnect(&g_instance->cx_handle, wlan_handle);
    if (result != 0) {
        LOG_ERROR("Failed to connect: %d", result);
        snprintf(g_instance->error_msg, sizeof(g_instance->error_msg),
                 "Failed to connect: %d", result);
        return result;
    }
    
    LOG_INFO("WiFi connect command sent successfully");
    return 0;
}

/**
 * Disconnect from WiFi
 * @return 0 on success, negative on error
 */
EMSCRIPTEN_KEEPALIVE
int ucx_wifi_disconnect(void) {
    if (!g_instance) {
        return -1;
    }
    
    LOG_INFO("Disconnecting WiFi");
    
    int32_t result = uCxWifiStationDisconnect(&g_instance->cx_handle);
    if (result != 0) {
        LOG_ERROR("Failed to disconnect: %d", result);
        return result;
    }
    
    return 0;
}

/**
 * Get WiFi connection info (IP address)
 * @param ip_str Output buffer for IP address string (min 16 bytes)
 * @return 0 on success, negative on error
 */
EMSCRIPTEN_KEEPALIVE
int ucx_wifi_get_ip(char* ip_str) {
    if (!g_instance || !ip_str) {
        return -1;
    }
    
    uSockIpAddress_t ip_addr;
    
    // Get IPv4 address using enum constant
    int32_t result = uCxWifiStationGetNetworkStatus(&g_instance->cx_handle, 
                                                   U_WIFI_NET_STATUS_ID_IPV4, 
                                                   &ip_addr);
    
    if (result != 0 || ip_addr.type != U_SOCK_ADDRESS_TYPE_V4) {
        strcpy(ip_str, "0.0.0.0");
        return -1;
    }
    
    // Convert to string
    uint32_t ipv4 = ip_addr.address.ipv4;
    snprintf(ip_str, 16, "%d.%d.%d.%d",
             (ipv4 >> 24) & 0xFF,
             (ipv4 >> 16) & 0xFF,
             (ipv4 >> 8) & 0xFF,
             ipv4 & 0xFF);
    
    LOG_INFO("IP address: %s", ip_str);
    return 0;
}

/* ----------------------------------------------------------------
 * SYSTEM FUNCTIONS
 * -------------------------------------------------------------- */

/**
 * Send raw AT command (for debugging)
 * @param command AT command string (without "AT" prefix or CRLF)
 * @param response Output buffer for response
 * @param response_size Size of response buffer
 * @return 0 on success, negative on error
 */
EMSCRIPTEN_KEEPALIVE
int ucx_send_at_command(const char* command, char* response, int response_size) {
    if (!g_instance || !command || !response) {
        return -1;
    }
    
    LOG_DEBUG("Sending AT command: AT%s", command);
    
    // Send command and get response
    uCxAtClientCmdBeginF(&g_instance->at_client, "AT", "%s", command);
    
    int32_t result = uCxAtClientCmdEnd(&g_instance->at_client);
    
    if (result < 0) {
        snprintf(response, response_size, "ERROR: %d", result);
        return result;
    }
    
    strncpy(response, "OK", response_size);
    return 0;
}

/**
 * Get module version info
 * @param version Output buffer for version string
 * @param version_size Size of version buffer
 * @return 0 on success, negative on error
 */
EMSCRIPTEN_KEEPALIVE
int ucx_get_version(char* version, int version_size) {
    if (!g_instance || !version) {
        return -1;
    }
    
    const char* version_str = NULL;
    
    // Get software version
    if (uCxGeneralGetSoftwareVersionBegin(&g_instance->cx_handle, &version_str)) {
        if (version_str) {
            strncpy(version, version_str, version_size - 1);
            version[version_size - 1] = '\0';
            LOG_INFO("Version: %s", version);
            return 0;
        }
    }
    
    return -1;
}

/* ----------------------------------------------------------------
 * BLUETOOTH FUNCTIONS
 * -------------------------------------------------------------- */

/**
 * Start BLE discovery (foreground, no duplicates, active mode)
 * @param timeout_ms Discovery duration in milliseconds (0 = default ~10s)
 * @return 0 on success, negative on error
 */
EMSCRIPTEN_KEEPALIVE
int ucx_bt_discovery_begin(int timeout_ms) {
    if (!g_instance) {
        return -1;
    }

    LOG_INFO("Starting BLE discovery (timeout=%d ms)", timeout_ms);

    if (timeout_ms > 0) {
        uCxBluetoothDiscovery3Begin(&g_instance->cx_handle,
                                    U_BT_DISCOVERY_TYPE_ALL_NO_DUPLICATES,
                                    U_BT_DISCOVERY_MODE_ACTIVE,
                                    timeout_ms);
    } else {
        uCxBluetoothDiscovery2Begin(&g_instance->cx_handle,
                                    U_BT_DISCOVERY_TYPE_ALL_NO_DUPLICATES,
                                    U_BT_DISCOVERY_MODE_ACTIVE);
    }

    return 0;
}

/**
 * Get next BLE discovery result.
 * @param addr_str   Output buffer (min 18 bytes) for BD address as "AABBCCDDEEFF" + type suffix
 * @param rssi       Output pointer for RSSI
 * @param name       Output buffer (min 64 bytes) for device name
 * @return 1 if result available, 0 if done, -1 on error
 */
EMSCRIPTEN_KEEPALIVE
int ucx_bt_discovery_get_next(char* addr_str, int* rssi, char* name) {
    if (!g_instance || !addr_str || !rssi || !name) {
        return -1;
    }

    uCxBtDiscovery_t result;
    bool has_more = uCxBluetoothDiscovery2GetNext(&g_instance->cx_handle, &result);

    if (!has_more) {
        return 0;
    }

    uCxBdAddressToString(&result.bd_addr, addr_str, 18);
    *rssi = result.rssi;
    if (result.device_name) {
        strncpy(name, result.device_name, 63);
        name[63] = '\0';
    } else {
        name[0] = '\0';
    }

    LOG_DEBUG("BLE device: %s rssi=%d name=\"%s\"", addr_str, *rssi, name);
    return 1;
}

/**
 * End BLE discovery.
 */
EMSCRIPTEN_KEEPALIVE
void ucx_bt_discovery_end(void) {
    if (!g_instance) return;
    uCxEnd(&g_instance->cx_handle);
    LOG_INFO("BLE discovery ended");
}

/**
 * Connect to a BLE peripheral.
 * @param addr_str BD address string (e.g. "AABBCCDDEEFF" with type suffix)
 * @param conn_handle Output pointer for the connection handle
 * @return 0 on success, negative on error
 */
EMSCRIPTEN_KEEPALIVE
int ucx_bt_connect(const char* addr_str, int* conn_handle) {
    if (!g_instance || !addr_str || !conn_handle) {
        return -1;
    }

    uBtLeAddress_t addr;
    if (uCxStringToBdAddress(addr_str, &addr) != 0) {
        LOG_ERROR("Invalid BLE address: %s", addr_str);
        return -1;
    }

    LOG_INFO("Connecting to BLE device: %s", addr_str);
    int32_t result = uCxBluetoothConnect(&g_instance->cx_handle, &addr);
    if (result < 0) {
        LOG_ERROR("BLE connect failed: %d", result);
        return result;
    }

    *conn_handle = result;
    LOG_INFO("BLE connected, handle=%d", *conn_handle);
    return 0;
}

/**
 * Disconnect a BLE connection.
 * @param conn_handle Connection handle from ucx_bt_connect
 * @return 0 on success, negative on error
 */
EMSCRIPTEN_KEEPALIVE
int ucx_bt_disconnect(int conn_handle) {
    if (!g_instance) return -1;

    LOG_INFO("Disconnecting BLE handle=%d", conn_handle);
    return uCxBluetoothDisconnect(&g_instance->cx_handle, conn_handle);
}

/* ----------------------------------------------------------------
 * GATT CLIENT FUNCTIONS
 * -------------------------------------------------------------- */

/**
 * Discover primary services on a connected peer.
 * @return 0 on success, negative on error
 */
EMSCRIPTEN_KEEPALIVE
int ucx_gatt_discover_services_begin(int conn_handle) {
    if (!g_instance) return -1;

    LOG_INFO("GATT discover services (conn=%d)", conn_handle);
    uCxGattClientDiscoverPrimaryServicesBegin(&g_instance->cx_handle, conn_handle);
    return 0;
}

/**
 * Get next discovered service.
 * @param start_handle Output pointer for service start handle
 * @param end_handle   Output pointer for service end handle
 * @param uuid_hex     Output buffer (min 37 bytes) for UUID as hex string
 * @return 1 if available, 0 if done, -1 on error
 */
EMSCRIPTEN_KEEPALIVE
int ucx_gatt_discover_services_get_next(int* start_handle, int* end_handle, char* uuid_hex) {
    if (!g_instance || !start_handle || !end_handle || !uuid_hex) return -1;

    uCxGattClientDiscoverPrimaryServices_t rsp;
    bool has_more = uCxGattClientDiscoverPrimaryServicesGetNext(&g_instance->cx_handle, &rsp);

    if (!has_more) return 0;

    *start_handle = rsp.start_handle;
    *end_handle = rsp.end_handle;

    // Convert UUID bytes to hex string
    for (size_t i = 0; i < rsp.uuid.length && i < 16; i++) {
        sprintf(uuid_hex + i * 2, "%02X", rsp.uuid.pData[i]);
    }
    uuid_hex[rsp.uuid.length * 2] = '\0';

    LOG_DEBUG("Service: start=%d end=%d uuid=%s", *start_handle, *end_handle, uuid_hex);
    return 1;
}

/**
 * End service discovery.
 */
EMSCRIPTEN_KEEPALIVE
void ucx_gatt_discover_services_end(void) {
    if (!g_instance) return;
    uCxEnd(&g_instance->cx_handle);
}

/**
 * Discover characteristics of a service.
 * @return 0 on success
 */
EMSCRIPTEN_KEEPALIVE
int ucx_gatt_discover_chars_begin(int conn_handle, int start_handle, int end_handle) {
    if (!g_instance) return -1;

    LOG_INFO("GATT discover chars (conn=%d, range=%d-%d)", conn_handle, start_handle, end_handle);
    uCxGattClientDiscoverServiceCharsBegin(&g_instance->cx_handle, conn_handle, start_handle, end_handle);
    return 0;
}

/**
 * Get next discovered characteristic.
 * @param attr_handle   Output: attribute handle
 * @param value_handle  Output: value handle (used for read/write)
 * @param properties    Output: properties bitmask
 * @param uuid_hex      Output: UUID hex string
 * @return 1 if available, 0 if done, -1 on error
 */
EMSCRIPTEN_KEEPALIVE
int ucx_gatt_discover_chars_get_next(int* attr_handle, int* value_handle,
                                      int* properties, char* uuid_hex) {
    if (!g_instance || !attr_handle || !value_handle || !properties || !uuid_hex)
        return -1;

    uCxGattClientDiscoverServiceChars_t rsp;
    bool has_more = uCxGattClientDiscoverServiceCharsGetNext(&g_instance->cx_handle, &rsp);

    if (!has_more) return 0;

    *attr_handle = rsp.attr_handle;
    *value_handle = rsp.value_handle;

    // Properties is a byte array but typically 1 byte bitmask
    *properties = 0;
    if (rsp.properties.length > 0 && rsp.properties.pData) {
        *properties = rsp.properties.pData[0];
    }

    for (size_t i = 0; i < rsp.uuid.length && i < 16; i++) {
        sprintf(uuid_hex + i * 2, "%02X", rsp.uuid.pData[i]);
    }
    uuid_hex[rsp.uuid.length * 2] = '\0';

    LOG_DEBUG("Char: attr=%d val=%d props=0x%02X uuid=%s",
              *attr_handle, *value_handle, *properties, uuid_hex);
    return 1;
}

/**
 * End characteristic discovery.
 */
EMSCRIPTEN_KEEPALIVE
void ucx_gatt_discover_chars_end(void) {
    if (!g_instance) return;
    uCxEnd(&g_instance->cx_handle);
}

/**
 * Read a GATT characteristic value.
 * @param conn_handle   Connection handle
 * @param value_handle  Characteristic value handle
 * @param out_data      Output buffer for data bytes
 * @param out_max       Max bytes to read
 * @return Number of bytes read, or negative on error
 */
EMSCRIPTEN_KEEPALIVE
int ucx_gatt_read(int conn_handle, int value_handle, uint8_t* out_data, int out_max) {
    if (!g_instance || !out_data) return -1;

    LOG_DEBUG("GATT read (conn=%d, val=%d)", conn_handle, value_handle);

    uByteArray_t data;
    bool ok = uCxGattClientReadBegin(&g_instance->cx_handle,
                                      conn_handle, value_handle, &data);
    if (!ok) {
        LOG_ERROR("GATT read failed");
        return -1;
    }

    int len = (int)data.length;
    if (len > out_max) len = out_max;
    memcpy(out_data, data.pData, len);

    return len;
}

/**
 * Write a GATT characteristic value (with response).
 * @param conn_handle   Connection handle
 * @param value_handle  Characteristic value handle
 * @param data          Data bytes to write
 * @param data_len      Number of bytes
 * @return 0 on success, negative on error
 */
EMSCRIPTEN_KEEPALIVE
int ucx_gatt_write(int conn_handle, int value_handle,
                    const uint8_t* data, int data_len) {
    if (!g_instance || !data) return -1;

    LOG_DEBUG("GATT write (conn=%d, val=%d, len=%d)", conn_handle, value_handle, data_len);
    return uCxGattClientWrite(&g_instance->cx_handle,
                               conn_handle, value_handle, data, data_len);
}

/**
 * Enable/disable GATT notifications or indications on a CCCD.
 * @param conn_handle Connection handle
 * @param cccd_handle CCCD descriptor handle (value_handle + 1 typically)
 * @param config      0=none, 1=notifications, 2=indications, 3=both
 * @return 0 on success, negative on error
 */
EMSCRIPTEN_KEEPALIVE
int ucx_gatt_config_write(int conn_handle, int cccd_handle, int config) {
    if (!g_instance) return -1;

    LOG_INFO("GATT config write (conn=%d, cccd=%d, cfg=%d)", conn_handle, cccd_handle, config);
    return uCxGattClientConfigWrite(&g_instance->cx_handle,
                                     conn_handle, cccd_handle,
                                     (uGattClientConfig_t)config);
}

/* ----------------------------------------------------------------
 * GATT SERVER FUNCTIONS
 * -------------------------------------------------------------- */

/**
 * Define a GATT service.
 * @param uuid_bytes  Raw UUID bytes (2 for 16-bit, 16 for 128-bit)
 * @param uuid_len    Length: 2 or 16
 * @param ser_handle  Output: service handle
 * @return 0 on success, negative on error
 */
EMSCRIPTEN_KEEPALIVE
int ucx_gatt_server_service_define(const uint8_t* uuid_bytes, int uuid_len,
                                    int* ser_handle) {
    if (!g_instance || !uuid_bytes || !ser_handle) return -1;

    int32_t handle;
    int32_t result = uCxGattServerServiceDefine(&g_instance->cx_handle,
                                                 uuid_bytes, uuid_len, &handle);
    if (result != 0) {
        LOG_ERROR("GATT server service define failed: %d", result);
        return result;
    }

    *ser_handle = handle;
    LOG_INFO("GATT server service defined, handle=%d", handle);
    return 0;
}

/**
 * Define a GATT server characteristic.
 * @param uuid_bytes  Raw UUID bytes
 * @param uuid_len    UUID length
 * @param props       Properties bitmask (0x02=read, 0x08=write, 0x10=notify, etc.)
 * @param initial_value  Initial value bytes (can be NULL)
 * @param initial_len    Initial value length
 * @param value_handle   Output: value handle
 * @param cccd_handle    Output: CCCD handle (-1 if no notify/indicate)
 * @return 0 on success, negative on error
 */
EMSCRIPTEN_KEEPALIVE
int ucx_gatt_server_char_define(const uint8_t* uuid_bytes, int uuid_len,
                                 int props,
                                 const uint8_t* initial_value, int initial_len,
                                 int* value_handle, int* cccd_handle) {
    if (!g_instance || !uuid_bytes || !value_handle || !cccd_handle) return -1;

    uint8_t props_byte = (uint8_t)props;
    uCxGattServerCharDefine_t rsp;

    int32_t result = uCxGattServerCharDefine5(
        &g_instance->cx_handle,
        uuid_bytes, uuid_len,
        &props_byte, 1,
        U_GATT_SERVER_READ_SECURITY_NONE,
        U_GATT_SERVER_WRITE_SECURITY_NONE,
        initial_value ? initial_value : (const uint8_t*)"", initial_len,
        &rsp);

    if (result != 0) {
        LOG_ERROR("GATT server char define failed: %d", result);
        return result;
    }

    *value_handle = rsp.value_handle;
    *cccd_handle = rsp.cccd_handle;
    LOG_INFO("GATT server char defined, val=%d, cccd=%d", rsp.value_handle, rsp.cccd_handle);
    return 0;
}

/**
 * Activate the GATT server (make services visible to peers).
 * @return 0 on success, negative on error
 */
EMSCRIPTEN_KEEPALIVE
int ucx_gatt_server_activate(void) {
    if (!g_instance) return -1;

    LOG_INFO("Activating GATT server");
    return uCxGattServerServiceActivate(&g_instance->cx_handle);
}

/**
 * Set a GATT server attribute value.
 * @param attr_handle Attribute handle (value_handle from char define)
 * @param value       Data bytes
 * @param value_len   Data length
 * @return 0 on success, negative on error
 */
EMSCRIPTEN_KEEPALIVE
int ucx_gatt_server_set_value(int attr_handle,
                               const uint8_t* value, int value_len) {
    if (!g_instance || !value) return -1;

    LOG_DEBUG("GATT server set value (handle=%d, len=%d)", attr_handle, value_len);
    return uCxGattServerSetAttrValue(&g_instance->cx_handle,
                                      attr_handle, value, value_len);
}

/**
 * Send a GATT notification to a connected client.
 * @param conn_handle  Connection handle
 * @param char_handle  Characteristic value handle
 * @param value        Data bytes
 * @param value_len    Data length
 * @return 0 on success, negative on error
 */
EMSCRIPTEN_KEEPALIVE
int ucx_gatt_server_send_notification(int conn_handle, int char_handle,
                                       const uint8_t* value, int value_len) {
    if (!g_instance || !value) return -1;

    LOG_DEBUG("GATT notify (conn=%d, char=%d, len=%d)", conn_handle, char_handle, value_len);
    return uCxGattServerSendNotification(&g_instance->cx_handle,
                                          conn_handle, char_handle,
                                          value, value_len);
}

/**
 * Start BLE advertising (so peripherals can find us / connect).
 * @return 0 on success, negative on error
 */
EMSCRIPTEN_KEEPALIVE
int ucx_bt_advertise_start(void) {
    if (!g_instance) return -1;
    LOG_INFO("Starting BLE advertising");
    return uCxBluetoothLegacyAdvertisementStart(&g_instance->cx_handle);
}

/**
 * Stop BLE advertising.
 * @return 0 on success, negative on error
 */
EMSCRIPTEN_KEEPALIVE
int ucx_bt_advertise_stop(void) {
    if (!g_instance) return -1;
    LOG_INFO("Stopping BLE advertising");
    return uCxBluetoothLegacyAdvertisementStop(&g_instance->cx_handle);
}
