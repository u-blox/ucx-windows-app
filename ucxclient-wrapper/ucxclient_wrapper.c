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

#include "ucx_wrapper_config.h"  // Must be first to override U_CX_PORT_PRINTF
#include "u_port_windows.h"  // Must be included before other ucxclient headers
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
    
    if (inst && inst->urc_callback) {
        // Ensure null-terminated string
        char urc_line[512];
        size_t copy_len = lineLength < sizeof(urc_line) - 1 ? lineLength : sizeof(urc_line) - 1;
        memcpy(urc_line, pLine, copy_len);
        urc_line[copy_len] = '\0';
        
        inst->urc_callback(urc_line, inst->urc_user_data);
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
    
    // Start WiFi scan (passive mode = 0)
    uCxWifiStationScan1Begin(&inst->cx_handle, 0);
    
    int count = 0;
    uCxWifiStationScan_t scan_result;
    
    // Get all scan results
    while (count < max_results && uCxWifiStationScan1GetNext(&inst->cx_handle, &scan_result)) {
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
    
    // End the command
    int32_t status = uCxEnd(&inst->cx_handle);
    if (status < 0) {
        snprintf(inst->error_msg, ERROR_MSG_SIZE, "WiFi scan failed with status: %d", status);
        return UCX_ERROR_AT_FAIL;
    }
    
    return count;
}
