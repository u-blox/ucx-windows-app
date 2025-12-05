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

// Note: ucx_wrapper_config.h and u_port_windows.h are force-included by CMake
#include "ucxclient_wrapper.h"
#include "u_cx_at_client.h"
#include "u_cx_log.h"
#include "u_cx.h"
#include "u_cx_wifi.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <Windows.h>

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

#define RX_BUFFER_SIZE 4096
#define URC_BUFFER_SIZE 2048
#define ERROR_MSG_SIZE 256

typedef struct {
    uCxAtClient_t at_client;
    uCxAtClientConfig_t config;
    uCxHandle_t cx_handle;
    uint8_t rx_buffer[RX_BUFFER_SIZE];
    uint8_t urc_buffer[URC_BUFFER_SIZE];
    char error_msg[ERROR_MSG_SIZE];
    ucx_urc_callback_t urc_callback;
    void* urc_user_data;
    ucx_log_callback_t log_callback;
    void* log_user_data;
} ucx_instance_t;

/* Global pointer to current instance (for log callback) */
static ucx_instance_t* g_current_instance = NULL;

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
    
    // Always output to debug console for Windows debugging
    OutputDebugStringA(buffer);
    
    // Forward to C# callback if set
    if (g_current_instance && g_current_instance->log_callback) {
        g_current_instance->log_callback(0, buffer, g_current_instance->log_user_data);
    }
    
    return ret;
}

static void internal_urc_callback(struct uCxAtClient *pClient, void *pTag, char *pLine,
                                   size_t lineLength, uint8_t *pBinaryData, size_t binaryDataLen)
{
    ucx_instance_t* inst = (ucx_instance_t*)pTag;
    
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

static void internal_log_callback(int level, const char* format, ...)
{
    // TODO: Forward to C# log callback if set
    // For now, just output to debug console
    (void)level;
    (void)format;
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
    ucx_instance_t* inst = (ucx_instance_t*)calloc(1, sizeof(ucx_instance_t));
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
        snprintf(inst->error_msg, ERROR_MSG_SIZE, 
                 "Failed to open UART %s at %d baud (error: %d)", 
                 port_name, baud_rate, result);
        uCxAtClientDeinit(&inst->at_client);
        free(inst);
        return NULL;
    }
    
    // Set internal URC callback
    uCxAtClientSetUrcCallback(&inst->at_client, internal_urc_callback, inst);
    
    // Initialize uCX handle
    uCxInit(&inst->at_client, &inst->cx_handle);
    
    // Register WiFi URC handlers with actual callbacks (not NULL)
    // This tells ucx_api to enable WiFi URCs and call our handlers
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
    
    ucx_instance_t* inst = (ucx_instance_t*)handle;
    
    // Close and deinit AT client
    uCxAtClientClose(&inst->at_client);
    uCxAtClientDeinit(&inst->at_client);
    
    free(inst);
}

bool ucx_is_connected(ucx_handle_t handle)
{
    if (!handle) {
        return false;
    }
    
    ucx_instance_t* inst = (ucx_instance_t*)handle;
    return inst->at_client.opened;
}

int ucx_send_at_command(ucx_handle_t handle, const char* command, 
                        char* response, int response_len, int timeout_ms)
{
    if (!handle || !command) {
        return UCX_ERROR_INVALID_PARAM;
    }
    
    ucx_instance_t* inst = (ucx_instance_t*)handle;
    
    if (!inst->at_client.opened) {
        strncpy(inst->error_msg, "Not connected", ERROR_MSG_SIZE);
        return UCX_ERROR_NOT_CONNECTED;
    }
    
    // Prepare AT command (add AT prefix if not present)
    char cmd_buf[256];
    if (strncmp(command, "AT", 2) != 0) {
        snprintf(cmd_buf, sizeof(cmd_buf), "AT%s", command);
    } else {
        strncpy(cmd_buf, command, sizeof(cmd_buf) - 1);
        cmd_buf[sizeof(cmd_buf) - 1] = '\0';
    }
    
    // Send command using ucxclient API
    uCxAtClientCmdBeginF(&inst->at_client, cmd_buf, "", timeout_ms);
    
    // Get response if buffer provided
    if (response && response_len > 0) {
        response[0] = '\0';  // Clear initially
        
        // Try to read response lines (for commands that return data)
        // This captures responses without a specific prefix (like AT+GMM, ATI, etc.)
        char* line = uCxAtClientCmdGetRspParamLine(&inst->at_client, NULL, NULL, NULL);
        if (line != NULL) {
            // Copy first response line to output buffer
            strncpy(response, line, response_len - 1);
            response[response_len - 1] = '\0';
            
            // Continue reading any additional response lines and append them
            size_t current_len = strlen(response);
            while ((line = uCxAtClientCmdGetRspParamLine(&inst->at_client, NULL, NULL, NULL)) != NULL) {
                if (current_len + 1 < (size_t)response_len) {
                    strncat(response, "\n", response_len - current_len - 1);
                    current_len++;
                }
                if (current_len < (size_t)response_len) {
                    strncat(response, line, response_len - current_len - 1);
                    current_len = strlen(response);
                }
            }
        }
    }
    
    // Wait for command completion and get status
    int32_t status = uCxAtClientCmdEnd(&inst->at_client);
    if (status < 0) {
        snprintf(inst->error_msg, ERROR_MSG_SIZE, "AT command failed with status: %d", status);
        return UCX_ERROR_AT_FAIL;
    }
    
    return UCX_OK;
}

void ucx_set_urc_callback(ucx_handle_t handle, ucx_urc_callback_t callback, void* user_data)
{
    if (!handle) {
        return;
    }
    
    ucx_instance_t* inst = (ucx_instance_t*)handle;
    inst->urc_callback = callback;
    inst->urc_user_data = user_data;
}

void ucx_set_log_callback(ucx_handle_t handle, ucx_log_callback_t callback, void* user_data)
{
    if (!handle) {
        return;
    }
    
    ucx_instance_t* inst = (ucx_instance_t*)handle;
    inst->log_callback = callback;
    inst->log_user_data = user_data;
}

const char* ucx_get_last_error(ucx_handle_t handle)
{
    if (!handle) {
        return "Invalid handle";
    }
    
    ucx_instance_t* inst = (ucx_instance_t*)handle;
    return inst->error_msg[0] != '\0' ? inst->error_msg : NULL;
}

int ucx_wifi_scan(ucx_handle_t handle, ucx_wifi_scan_result_t* results, 
                  int max_results, int timeout_ms)
{
    if (!handle || !results || max_results <= 0) {
        return UCX_ERROR_INVALID_PARAM;
    }
    
    ucx_instance_t* inst = (ucx_instance_t*)handle;
    
    if (!inst->at_client.opened) {
        strncpy(inst->error_msg, "Not connected", ERROR_MSG_SIZE);
        return UCX_ERROR_NOT_CONNECTED;
    }
    
    ucx_wrapper_printf("Starting WiFi scan (passive mode)...\n");
    
    // Start WiFi scan (passive mode = 0)
    uCxWifiStationScan1Begin(&inst->cx_handle, 0);
    
    int count = 0;
    uCxWifiStationScan_t scan_result;
    
    // Get all scan results
    while (count < max_results && uCxWifiStationScan1GetNext(&inst->cx_handle, &scan_result)) {
        ucx_wrapper_printf("Found network: %s (RSSI: %d)\n", scan_result.ssid, scan_result.rssi);
        
        // Copy BSSID
        memcpy(results[count].bssid, scan_result.bssid.address, 6);
        
        // Copy SSID (ensure null termination)
        strncpy(results[count].ssid, scan_result.ssid, sizeof(results[count].ssid) - 1);
        results[count].ssid[sizeof(results[count].ssid) - 1] = '\0';
        
        // Copy other fields
        results[count].channel = scan_result.channel;
        results[count].rssi = scan_result.rssi;
        results[count].auth_suites = scan_result.authentication_suites;
        results[count].unicast_ciphers = scan_result.unicast_ciphers;
        results[count].group_ciphers = scan_result.group_ciphers;
        
        count++;
    }
    
    ucx_wrapper_printf("WiFi scan found %d networks\n", count);
    
    // End the command
    int32_t status = uCxEnd(&inst->cx_handle);
    if (status < 0) {
        snprintf(inst->error_msg, ERROR_MSG_SIZE, "WiFi scan failed with status: %d", status);
        ucx_wrapper_printf("WiFi scan end failed: %d\n", status);
        return UCX_ERROR_AT_FAIL;
    }
    
    ucx_wrapper_printf("WiFi scan completed successfully\n");
    return count;
}

// WiFi connect function
int ucx_wifi_connect(ucx_handle_t handle, const char* ssid, const char* password, int timeout_ms)
{
    ucx_instance_t* inst = (ucx_instance_t*)handle;
    if (!inst || !ssid) {
        return UCX_ERROR_INVALID_PARAM;
    }
    
    ucx_wrapper_printf("Connecting to WiFi: %s\n", ssid);
    
    // Use wlan_handle 0 (default)
    int32_t wlan_handle = 0;
    int32_t status;
    
    // Set authentication (password) - must be done BEFORE setting connection params
    if (password && strlen(password) > 0) {
        // Use WPA/WPA2 with WPA2 threshold (following http_example pattern)
        status = uCxWifiStationSetSecurityWpa(&inst->cx_handle, wlan_handle, password, U_WIFI_WPA_THRESHOLD_WPA2);
        if (status < 0) {
            snprintf(inst->error_msg, ERROR_MSG_SIZE, "Failed to set security: %d", status);
            ucx_wrapper_printf("Set security failed: %d\n", status);
            return UCX_ERROR_AT_FAIL;
        }
        ucx_wrapper_printf("Set WPA/WPA2 security\n");
    } else {
        // Open network
        status = uCxWifiStationSetSecurityOpen(&inst->cx_handle, wlan_handle);
        if (status < 0) {
            snprintf(inst->error_msg, ERROR_MSG_SIZE, "Failed to set open security: %d", status);
            ucx_wrapper_printf("Set open security failed: %d\n", status);
            return UCX_ERROR_AT_FAIL;
        }
        ucx_wrapper_printf("Set open security\n");
    }
    
    // Set connection parameters (SSID) - must be done AFTER security
    status = uCxWifiStationSetConnectionParams(&inst->cx_handle, wlan_handle, ssid);
    if (status < 0) {
        snprintf(inst->error_msg, ERROR_MSG_SIZE, "Failed to set connection params: %d", status);
        ucx_wrapper_printf("Set connection params failed: %d\n", status);
        return UCX_ERROR_AT_FAIL;
    }
    ucx_wrapper_printf("Set connection params (SSID: %s)\n", ssid);
    
    // Initiate connection
    status = uCxWifiStationConnect(&inst->cx_handle, wlan_handle);
    if (status < 0) {
        snprintf(inst->error_msg, ERROR_MSG_SIZE, "WiFi connect failed: %d", status);
        ucx_wrapper_printf("WiFi connect failed: %d\n", status);
        return UCX_ERROR_AT_FAIL;
    }
    
    ucx_wrapper_printf("WiFi connection initiated successfully\n");
    return UCX_OK;
}

// WiFi disconnect function
int ucx_wifi_disconnect(ucx_handle_t handle)
{
    ucx_instance_t* inst = (ucx_instance_t*)handle;
    if (!inst) {
        return UCX_ERROR_INVALID_PARAM;
    }
    
    ucx_wrapper_printf("Disconnecting from WiFi\n");
    
    int32_t status = uCxWifiStationDisconnect(&inst->cx_handle);
    if (status < 0) {
        snprintf(inst->error_msg, ERROR_MSG_SIZE, "WiFi disconnect failed: %d", status);
        ucx_wrapper_printf("WiFi disconnect failed: %d\n", status);
        return UCX_ERROR_AT_FAIL;
    }
    
    ucx_wrapper_printf("WiFi disconnected successfully\n");
    return UCX_OK;
}

// Get WiFi connection info
int ucx_wifi_get_connection_info(ucx_handle_t handle, ucx_wifi_connection_info_t* info)
{
    ucx_instance_t* inst = (ucx_instance_t*)handle;
    if (!inst || !info) {
        return UCX_ERROR_INVALID_PARAM;
    }
    
    // Initialize all fields to empty/zero
    memset(info, 0, sizeof(ucx_wifi_connection_info_t));
    
    // Get IP address
    uSockIpAddress_t ipAddr;
    if (uCxWifiStationGetNetworkStatus(&inst->cx_handle, U_WIFI_NET_STATUS_ID_IPV4, &ipAddr) == 0) {
        uCxIpAddressToString(&ipAddr, info->ip_address, sizeof(info->ip_address));
    }
    
    // Get subnet mask
    if (uCxWifiStationGetNetworkStatus(&inst->cx_handle, U_WIFI_NET_STATUS_ID_SUBNET, &ipAddr) == 0) {
        uCxIpAddressToString(&ipAddr, info->subnet_mask, sizeof(info->subnet_mask));
    }
    
    // Get gateway
    if (uCxWifiStationGetNetworkStatus(&inst->cx_handle, U_WIFI_NET_STATUS_ID_GATE_WAY, &ipAddr) == 0) {
        uCxIpAddressToString(&ipAddr, info->gateway, sizeof(info->gateway));
    }
    
    // Get channel
    uCxWifiStationStatus_t channelStatus;
    if (uCxWifiStationStatusBegin(&inst->cx_handle, U_WIFI_STATUS_ID_CHANNEL, &channelStatus)) {
        info->channel = channelStatus.rsp.StatusIdInt.int_val;
        uCxEnd(&inst->cx_handle);
    }
    
    // Get RSSI
    uCxWifiStationStatus_t rssiStatus;
    if (uCxWifiStationStatusBegin(&inst->cx_handle, U_WIFI_STATUS_ID_RSSI, &rssiStatus)) {
        info->rssi = rssiStatus.rsp.StatusIdInt.int_val;
        uCxEnd(&inst->cx_handle);
    }
    
    ucx_wrapper_printf("Connection info: IP=%s, Gateway=%s, Channel=%d, RSSI=%d dBm\n",
                      info->ip_address, info->gateway, info->channel, info->rssi);
    
    return UCX_OK;
}

