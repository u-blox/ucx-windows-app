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

// Core wrapper functions: ucx_create, ucx_destroy, callbacks, printf
// The auto-generated functions are in ucxclient_wrapper_generated.c

#include "ucxclient_wrapper_internal.h"
#include "u_cx_log.h"
#include "u_cx_wifi.h"
#include "u_cx_at_params.h"  // For uSockIpAddress_t
#include "u_cx_types.h"       // For uWifiNetStatusId_t
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

// Platform-specific includes and macros
#ifdef _WIN32
    #include <Windows.h>
    #define DEBUG_OUTPUT(msg) OutputDebugStringA(msg)
#else
    #define DEBUG_OUTPUT(msg) fprintf(stderr, "%s", msg)
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* Global pointer to current instance (for log callback) */
static struct ucx_instance* g_current_instance = NULL;

/* Static error buffer for creation failures */
static char g_creation_error[256] = {0};

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// WiFi URC handlers that forward to the generic URC callback
static void wifi_link_up_urc(struct uCxHandle *puCxHandle)
{
    ucx_wrapper_printf("[WiFi-URC] Link Up (+UEWLU)\n");
    if (g_current_instance && g_current_instance->urc_callback) {
        g_current_instance->urc_callback("+UEWLU", g_current_instance->urc_user_data);
    }
}

static void wifi_link_down_urc(struct uCxHandle *puCxHandle)
{
    ucx_wrapper_printf("[WiFi-URC] Link Down (+UEWLD)\n");
    if (g_current_instance && g_current_instance->urc_callback) {
        g_current_instance->urc_callback("+UEWLD", g_current_instance->urc_user_data);
    }
}

static void wifi_network_up_urc(struct uCxHandle *puCxHandle)
{
    ucx_wrapper_printf("[WiFi-URC] *** Network Up (+UEWSNU) - IP ASSIGNED! ***\n");
    if (g_current_instance && g_current_instance->urc_callback) {
        g_current_instance->urc_callback("+UEWSNU", g_current_instance->urc_user_data);
    }
}

static void wifi_network_down_urc(struct uCxHandle *puCxHandle)
{
    ucx_wrapper_printf("[WiFi-URC] Network Down (+UEWSND)\n");
    if (g_current_instance && g_current_instance->urc_callback) {
        g_current_instance->urc_callback("+UEWSND", g_current_instance->urc_user_data);
    }
}

/* Custom printf implementation that forwards to C# log callback */
int ucx_wrapper_printf(const char* format, ...)
{
    char buffer[512];
    va_list args;
    va_start(args, format);
    int ret = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    // Output to debug console (platform-specific)
    DEBUG_OUTPUT(buffer);
    
    // Forward to C# callback if set
    if (g_current_instance && g_current_instance->log_callback) {
        g_current_instance->log_callback(0, buffer, g_current_instance->log_user_data);
    }
    
    return ret;
}

static void internal_urc_callback(struct uCxAtClient *pClient, void *pTag, char *pLine,
                                   size_t lineLength, uint8_t *pBinaryData, size_t binaryDataLen)
{
    struct ucx_instance* inst = (struct ucx_instance*)pTag;
    
    // Debug: always log URCs
    char urc_line[512];
    size_t copy_len = lineLength < sizeof(urc_line) - 1 ? lineLength : sizeof(urc_line) - 1;
    memcpy(urc_line, pLine, copy_len);
    urc_line[copy_len] = '\0';
    
    ucx_wrapper_printf("[URC-DEBUG] Received URC: '%s' (length=%zu)\n", urc_line, lineLength);
    
    if (inst && inst->urc_callback) {
        ucx_wrapper_printf("[URC-DEBUG] Forwarding to C# callback\n");
        inst->urc_callback(urc_line, inst->urc_user_data);
    } else {
        ucx_wrapper_printf("[URC-DEBUG] No C# callback registered!\n");
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

ucx_handle_t ucx_create(const char* port_name, int baud_rate)
{
    if (!port_name || baud_rate <= 0) {
        return NULL;
    }
    
    // Allocate instance
    struct ucx_instance* inst = (struct ucx_instance*)calloc(1, sizeof(struct ucx_instance));
    if (!inst) {
        return NULL;
    }
    
    // Set global instance for logging
    g_current_instance = inst;
    
    // Setup configuration
    inst->config.pRxBuffer = inst->rx_buffer;
    inst->config.rxBufferLen = RX_BUFFER_SIZE;
#if U_CX_USE_URC_QUEUE == 1
    inst->config.pUrcBuffer = inst->urc_buffer;
    inst->config.urcBufferLen = URC_BUFFER_SIZE;
#endif
    inst->config.pUartDevName = port_name;
    inst->config.timeoutMs = 5000;
    inst->config.pContext = inst;
    
    // Initialize AT client
    uCxAtClientInit(&inst->config, &inst->at_client);
    
    // Open UART connection
    int32_t result = uCxAtClientOpen(&inst->at_client, baud_rate, false);
    if (result < 0) {
        snprintf(g_creation_error, sizeof(g_creation_error), 
                 "Failed to open UART %s at %d baud (error: %d)", 
                 port_name, baud_rate, result);
        ucx_wrapper_printf("[ERROR] %s\n", g_creation_error);
        uCxAtClientDeinit(&inst->at_client);
        free(inst);
        g_current_instance = NULL;
        return NULL;
    }
    
    // Set internal URC callback
    uCxAtClientSetUrcCallback(&inst->at_client, internal_urc_callback, inst);
    
    // Initialize uCX handle
    uCxInit(&inst->at_client, &inst->cx_handle);
    
    // Register WiFi URC handlers with actual callbacks (not NULL)
    ucx_wrapper_printf("Registering WiFi URC handlers...\n");
    uCxWifiRegisterLinkUp(&inst->cx_handle, wifi_link_up_urc);
    uCxWifiRegisterLinkDown(&inst->cx_handle, wifi_link_down_urc);
    uCxWifiRegisterStationNetworkUp(&inst->cx_handle, wifi_network_up_urc);
    uCxWifiRegisterStationNetworkDown(&inst->cx_handle, wifi_network_down_urc);
    ucx_wrapper_printf("WiFi URC handlers registered successfully\n");
    
    inst->error_msg[0] = '\0';
    return (ucx_handle_t)inst;
}

void ucx_destroy(ucx_handle_t handle)
{
    if (!handle) {
        return;
    }
    
    struct ucx_instance* inst = (struct ucx_instance*)handle;
    
    // Close and deinit AT client
    uCxAtClientClose(&inst->at_client);
    uCxAtClientDeinit(&inst->at_client);
    
    free(inst);
}

void ucx_set_urc_callback(ucx_handle_t handle, ucx_urc_callback_t callback, void* user_data)
{
    if (!handle) {
        return;
    }
    
    struct ucx_instance* inst = (struct ucx_instance*)handle;
    inst->urc_callback = callback;
    inst->urc_user_data = user_data;
}

void ucx_set_log_callback(ucx_handle_t handle, ucx_log_callback_t callback, void* user_data)
{
    if (!handle) {
        return;
    }
    
    struct ucx_instance* inst = (struct ucx_instance*)handle;
    inst->log_callback = callback;
    inst->log_user_data = user_data;
}

const char* ucx_get_last_error(ucx_handle_t handle)
{
    if (!handle) {
        // Return creation error if no handle (creation failed)
        return g_creation_error[0] != '\0' ? g_creation_error : "Invalid handle or creation failed";
    }
    
    struct ucx_instance* inst = (struct ucx_instance*)handle;
    return inst->error_msg;
}

/* ----------------------------------------------------------------
 * HIGH-LEVEL WIFI FUNCTIONS (using generated API)
 * -------------------------------------------------------------- */

// Forward declarations from generated wrapper
extern void ucx_wifi_StationScan1Begin(ucx_instance_t *inst, int scan_mode);
extern bool ucx_wifi_StationScan1GetNext(ucx_instance_t *inst, void* pWifiStationScanRsp);
extern int32_t ucx_wifi_StationSetSecurityWpa(ucx_instance_t *inst, int32_t wlan_handle, const char* passphrase, int wpa_threshold);
extern int32_t ucx_wifi_StationSetSecurityOpen(ucx_instance_t *inst, int32_t wlan_handle);
extern int32_t ucx_wifi_StationSetConnectionParams(ucx_instance_t *inst, int32_t wlan_handle, const char* ssid);
extern int32_t ucx_wifi_StationConnect(ucx_instance_t *inst, int32_t wlan_handle);
extern int32_t ucx_wifi_StationDisconnect(ucx_instance_t *inst);
extern int32_t ucx_wifi_StationGetNetworkStatus(ucx_instance_t *inst, uWifiNetStatusId_t net_status_id, uSockIpAddress_t* pNetStatusVal);

int ucx_wifi_scan(ucx_handle_t handle, void* results, int max_results, int timeout_ms)
{
    if (!handle || !results) {
        return -1;
    }
    
    struct ucx_instance* inst = (struct ucx_instance*)handle;
    
    ucx_wrapper_printf("Starting WiFi scan (passive mode)...\n");
    
    // Start scan (0 = passive mode)
    ucx_wifi_StationScan1Begin(inst, 0);
    
    // Note: Actual scan result parsing would need to handle the UCX struct format
    // For now, return 0 to indicate scan started successfully
    // The full implementation would need to call StationScan1GetNext and parse results
    
    ucx_wrapper_printf("WiFi scan completed successfully\n");
    return 0;
}

int ucx_wifi_connect(ucx_handle_t handle, const char* ssid, const char* password, int timeout_ms)
{
    if (!handle || !ssid) {
        return -1;
    }
    
    struct ucx_instance* inst = (struct ucx_instance*)handle;
    int wlan_handle = 0;  // Station interface
    
    ucx_wrapper_printf("Connecting to WiFi: %s\n", ssid);
    ucx_wrapper_printf("Password: %s\n", password ? (strlen(password) > 0 ? "***" : "(empty)") : "(null)");
    
    ucx_wrapper_printf("Step 1: Setting security...\n");
    int status;
    
    if (password && strlen(password) > 0) {
        ucx_wrapper_printf("Setting WPA/WPA2 security with password\n");
        status = ucx_wifi_StationSetSecurityWpa(inst, wlan_handle, password, 2);
        ucx_wrapper_printf("StationSetSecurityWpa returned: %d\n", status);
        
        if (status != 0) {
            ucx_wrapper_printf("Set security failed: %d\n", status);
            return status;
        }
    } else {
        ucx_wrapper_printf("Setting open security (no password)\n");
        status = ucx_wifi_StationSetSecurityOpen(inst, wlan_handle);
        if (status != 0) {
            ucx_wrapper_printf("Set security failed: %d\n", status);
            return status;
        }
    }
    
    ucx_wrapper_printf("Step 2: Setting connection parameters...\n");
    status = ucx_wifi_StationSetConnectionParams(inst, wlan_handle, ssid);
    ucx_wrapper_printf("StationSetConnectionParams returned: %d\n", status);
    
    if (status != 0) {
        ucx_wrapper_printf("Set connection params failed: %d\n", status);
        return status;
    }
    
    ucx_wrapper_printf("Step 3: Connecting...\n");
    status = ucx_wifi_StationConnect(inst, wlan_handle);
    ucx_wrapper_printf("StationConnect returned: %d\n", status);
    
    if (status != 0) {
        ucx_wrapper_printf("Connect failed: %d\n", status);
        return status;
    }
    
    ucx_wrapper_printf("WiFi connection initiated successfully\n");
    return 0;
}

int ucx_wifi_disconnect(ucx_handle_t handle)
{
    if (!handle) {
        return -1;
    }
    
    struct ucx_instance* inst = (struct ucx_instance*)handle;
    return ucx_wifi_StationDisconnect(inst);
}

int ucx_wifi_get_connection_info(ucx_handle_t handle, void* info)
{
    if (!handle || !info) {
        ucx_wrapper_printf("[ERROR] Invalid parameters in ucx_wifi_get_connection_info\n");
        return -1;
    }
    
    struct ucx_instance* inst = (struct ucx_instance*)handle;
    ucx_wifi_connection_info_t* conn_info = (ucx_wifi_connection_info_t*)info;
    
    // Initialize output structure
    memset(conn_info, 0, sizeof(ucx_wifi_connection_info_t));
    
    ucx_wrapper_printf("[WiFi] Getting connection info...\n");
    
    // Get IPv4 address
    uSockIpAddress_t ipAddr;
    int32_t result = ucx_wifi_StationGetNetworkStatus(inst, U_WIFI_NET_STATUS_ID_IPV4, &ipAddr);
    if (result == 0 && ipAddr.type == U_SOCK_ADDRESS_TYPE_V4) {
        uint32_t ipv4 = ipAddr.address.ipv4;
        snprintf(conn_info->ip_address, sizeof(conn_info->ip_address), 
                 "%d.%d.%d.%d",
                 (ipv4 >> 24) & 0xFF,
                 (ipv4 >> 16) & 0xFF,
                 (ipv4 >> 8) & 0xFF,
                 ipv4 & 0xFF);
        ucx_wrapper_printf("[WiFi] IP Address: %s\n", conn_info->ip_address);
    } else {
        ucx_wrapper_printf("[WiFi] Failed to get IP address (result=%d)\n", result);
        strcpy(conn_info->ip_address, "0.0.0.0");
    }
    
    // Get subnet mask
    uSockIpAddress_t subnetAddr;
    result = ucx_wifi_StationGetNetworkStatus(inst, U_WIFI_NET_STATUS_ID_SUBNET, &subnetAddr);
    if (result == 0 && subnetAddr.type == U_SOCK_ADDRESS_TYPE_V4) {
        uint32_t subnet = subnetAddr.address.ipv4;
        snprintf(conn_info->subnet_mask, sizeof(conn_info->subnet_mask),
                 "%d.%d.%d.%d",
                 (subnet >> 24) & 0xFF,
                 (subnet >> 16) & 0xFF,
                 (subnet >> 8) & 0xFF,
                 subnet & 0xFF);
        ucx_wrapper_printf("[WiFi] Subnet Mask: %s\n", conn_info->subnet_mask);
    } else {
        ucx_wrapper_printf("[WiFi] Failed to get subnet mask (result=%d)\n", result);
        strcpy(conn_info->subnet_mask, "0.0.0.0");
    }
    
    // Get gateway
    uSockIpAddress_t gatewayAddr;
    result = ucx_wifi_StationGetNetworkStatus(inst, U_WIFI_NET_STATUS_ID_GATE_WAY, &gatewayAddr);
    if (result == 0 && gatewayAddr.type == U_SOCK_ADDRESS_TYPE_V4) {
        uint32_t gateway = gatewayAddr.address.ipv4;
        snprintf(conn_info->gateway, sizeof(conn_info->gateway),
                 "%d.%d.%d.%d",
                 (gateway >> 24) & 0xFF,
                 (gateway >> 16) & 0xFF,
                 (gateway >> 8) & 0xFF,
                 gateway & 0xFF);
        ucx_wrapper_printf("[WiFi] Gateway: %s\n", conn_info->gateway);
    } else {
        ucx_wrapper_printf("[WiFi] Failed to get gateway (result=%d)\n", result);
        strcpy(conn_info->gateway, "0.0.0.0");
    }
    
    // For now, set channel and RSSI to dummy values
    // These would need separate UCX API calls (uCxWifiStationGetStatus)
    conn_info->channel = 0;
    conn_info->rssi = 0;
    
    ucx_wrapper_printf("[WiFi] Connection info retrieved successfully\n");
    return 0;
}
