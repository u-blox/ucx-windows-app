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

#ifndef UCXCLIENT_WRAPPER_H
#define UCXCLIENT_WRAPPER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

typedef void* ucx_handle_t;

typedef enum {
    UCX_OK = 0,
    UCX_ERROR_INVALID_PARAM = -1,
    UCX_ERROR_NO_MEMORY = -2,
    UCX_ERROR_TIMEOUT = -3,
    UCX_ERROR_NOT_CONNECTED = -4,
    UCX_ERROR_AT_FAIL = -5,
    UCX_ERROR_UART_OPEN_FAIL = -6,
} ucx_error_t;

/* Callback for unsolicited result codes (URCs) */
typedef void (*ucx_urc_callback_t)(const char* urc_line, void* user_data);

/* Callback for log messages */
typedef void (*ucx_log_callback_t)(int level, const char* message, void* user_data);

/* WiFi scan result structure */
typedef struct {
    uint8_t bssid[6];           /* MAC address */
    char ssid[33];              /* SSID (max 32 chars + null terminator) */
    int32_t channel;            /* Channel */
    int32_t rssi;               /* Signal strength */
    int32_t auth_suites;        /* Authentication suites bitmask */
    int32_t unicast_ciphers;    /* Unicast ciphers bitmask */
    int32_t group_ciphers;      /* Group ciphers bitmask */
} ucx_wifi_scan_result_t;

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

/**
 * Create a UCX client instance and open the serial port.
 * 
 * @param port_name  COM port name (e.g., "COM3")
 * @param baud_rate  Baud rate (typically 115200)
 * @return Handle to UCX instance, or NULL on error
 */
ucx_handle_t ucx_create(const char* port_name, int baud_rate);

/**
 * Destroy UCX client instance and close the serial port.
 * 
 * @param handle  UCX handle from ucx_create()
 */
void ucx_destroy(ucx_handle_t handle);

/**
 * Check if UCX client is connected.
 * 
 * @param handle  UCX handle
 * @return true if connected, false otherwise
 */
bool ucx_is_connected(ucx_handle_t handle);

/**
 * Send an AT command and get the response.
 * 
 * @param handle        UCX handle
 * @param command       AT command to send (without AT prefix or CRLF)
 * @param response      Buffer to store response
 * @param response_len  Size of response buffer
 * @param timeout_ms    Timeout in milliseconds
 * @return UCX_OK on success, error code otherwise
 */
int ucx_send_at_command(ucx_handle_t handle, const char* command, 
                        char* response, int response_len, int timeout_ms);

/**
 * Set callback for unsolicited result codes (URCs).
 * 
 * @param handle     UCX handle
 * @param callback   Callback function
 * @param user_data  User data to pass to callback
 */
void ucx_set_urc_callback(ucx_handle_t handle, ucx_urc_callback_t callback, void* user_data);

/**
 * Set callback for log messages.
 * 
 * @param handle     UCX handle
 * @param callback   Callback function
 * @param user_data  User data to pass to callback
 */
void ucx_set_log_callback(ucx_handle_t handle, ucx_log_callback_t callback, void* user_data);

/**
 * Get last error message.
 * 
 * @param handle  UCX handle
 * @return Error message string, or NULL if no error
 */
const char* ucx_get_last_error(ucx_handle_t handle);

/**
 * Perform WiFi scan and get results.
 * 
 * @param handle        UCX handle
 * @param results       Array to store scan results
 * @param max_results   Maximum number of results to return
 * @param timeout_ms    Timeout in milliseconds
 * @return Number of networks found (>=0) on success, error code (<0) on failure
 */
int ucx_wifi_scan(ucx_handle_t handle, ucx_wifi_scan_result_t* results, 
                  int max_results, int timeout_ms);

/**
 * Connect to a WiFi network.
 * 
 * @param handle        UCX handle
 * @param ssid          Network SSID
 * @param password      Network password (NULL for open networks)
 * @param timeout_ms    Timeout in milliseconds
 * @return UCX_OK on success, error code otherwise
 */
int ucx_wifi_connect(ucx_handle_t handle, const char* ssid, const char* password, int timeout_ms);

/**
 * Disconnect from WiFi network.
 * 
 * @param handle  UCX handle
 * @return UCX_OK on success, error code otherwise
 */
int ucx_wifi_disconnect(ucx_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* UCXCLIENT_WRAPPER_H */
