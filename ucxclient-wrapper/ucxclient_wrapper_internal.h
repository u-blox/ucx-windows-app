/*
 * Internal header shared between core and generated wrapper code
 * DO NOT include from external code - use ucxclient_wrapper.h instead
 */

#ifndef UCXCLIENT_WRAPPER_INTERNAL_H
#define UCXCLIENT_WRAPPER_INTERNAL_H

#include "ucxclient_wrapper.h"
#include "u_cx_at_client.h"
#include "u_cx.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

#define RX_BUFFER_SIZE 4096
#define URC_BUFFER_SIZE 2048
#define ERROR_MSG_SIZE 256

// Full definition of ucx_instance (forward declared in public header)
struct ucx_instance {
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
};

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

// Internal printf forwarding (defined in core, used by generated code)
extern int ucx_wrapper_printf(const char* fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* UCXCLIENT_WRAPPER_INTERNAL_H */
