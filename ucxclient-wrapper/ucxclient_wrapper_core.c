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
#include "u_cx_wifi.h"  // For uCxWifiStationScan_t
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

void ucx_End(ucx_handle_t handle)
{
    if (!handle) {
        return;
    }
    
    struct ucx_instance* inst = (struct ucx_instance*)handle;
    uCxEnd(&inst->cx_handle);
}

/* ----------------------------------------------------------------
 * NOTE: WiFi functions are now 100% auto-generated!
 * -------------------------------------------------------------- */

// All WiFi/Bluetooth/HTTP/MQTT/etc. functions are in ucxclient_wrapper_generated.c
// Use UcxNativeGenerated class from C# to call them directly.
// This file only contains core instance management functions.
