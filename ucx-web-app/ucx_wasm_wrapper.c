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
