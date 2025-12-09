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
 * @file u_port_web.c
 * @brief WebAssembly/Browser port implementation for ucxclient
 * 
 * This file bridges the ucxclient UART abstraction to the Web Serial API.
 * It provides the platform-specific implementation needed to run ucxclient
 * in a browser using WebAssembly.
 */

#include <emscripten.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* ----------------------------------------------------------------
 * FORWARD DECLARATIONS
 * -------------------------------------------------------------- */
int32_t uPortGetTickTimeMs(void);

/* ----------------------------------------------------------------
 * JAVASCRIPT FUNCTIONS (implemented in library.js)
 * These are JavaScript functions that can be called from C code
 * -------------------------------------------------------------- */

/**
 * Write data to Web Serial port
 * @param data Pointer to data buffer
 * @param length Number of bytes to write
 * @return Number of bytes written, or -1 on error
 */
EM_JS(int, js_serial_write, (const char* data, int length), {
    try {
        // Create a copy of the data from WASM memory
        const buffer = new Uint8Array(length);
        for (let i = 0; i < length; i++) {
            buffer[i] = HEAPU8[data + i];
        }
        return Module.serialWrite(buffer);
    } catch (e) {
        console.error('js_serial_write error:', e);
        return -1;
    }
});

/**
 * Read data from Web Serial port receive buffer
 * @param buffer Pointer to output buffer
 * @param maxLength Maximum bytes to read
 * @return Number of bytes read, or -1 on error
 */
EM_JS(int, js_serial_read, (char* buffer, int maxLength), {
    try {
        const jsBuffer = Module.serialRead(maxLength);
        if (jsBuffer && jsBuffer.length > 0) {
            // Copy data into WASM memory
            for (let i = 0; i < jsBuffer.length; i++) {
                HEAPU8[buffer + i] = jsBuffer[i];
            }
            return jsBuffer.length;
        }
        return 0;
    } catch (e) {
        console.error('js_serial_read error:', e);
        return -1;
    }
});

/**
 * Check how many bytes are available in the receive buffer
 * @return Number of bytes available
 */
EM_JS(int, js_serial_available, (), {
    try {
        return Module.serialAvailable();
    } catch (e) {
        console.error('js_serial_available error:', e);
        return 0;
    }
});

/**
 * Sleep/delay for specified milliseconds
 * @param ms Milliseconds to sleep
 */
EM_JS(void, js_sleep, (int ms), {
    // In browser, we can't actually block, so this is a no-op
    // Actual waiting is handled by JavaScript async/await
});

/* ----------------------------------------------------------------
 * PORT INITIALIZATION
 * -------------------------------------------------------------- */

/**
 * Initialize the port layer (no-op for web)
 */
void uPortInit(void) {
    // No initialization needed for WebAssembly
}

/**
 * Deinitialize the port layer (no-op for web)
 */
void uPortDeinit(void) {
    // No cleanup needed for WebAssembly
}

/* ----------------------------------------------------------------
 * UCX PORT IMPLEMENTATION
 * These functions implement the ucxclient port interface
 * -------------------------------------------------------------- */

/**
 * Open a UART device (Web Serial port)
 * 
 * In the browser environment, the serial port is already opened
 * in JavaScript before initializing the WASM module, so this
 * function just validates the connection.
 */
void* uPortUartOpen(const char *pDevName, int32_t baudRate, bool useFlowControl) {
    // Web Serial port is already opened in JavaScript
    // Just log the configuration for debugging
    printf("[u_port_web] UART open: dev=%s, baudRate=%d, flowControl=%d\n", 
           pDevName ? pDevName : "(null)", baudRate, useFlowControl);
    return (void*)1; // Return non-NULL handle for success
}

/**
 * Close a UART device (Web Serial port)
 * 
 * Cleanup is handled in JavaScript when the page unloads,
 * so this is mostly a no-op.
 */
void uPortUartClose(void* handle) {
    printf("[u_port_web] UART close: handle=%p\n", handle);
    // Actual port closing is handled in JavaScript
}

/**
 * Write data to UART (Web Serial)
 * 
 * Sends data through the Web Serial API by calling the
 * JavaScript bridge function.
 */
int32_t uPortUartWrite(void* handle, const void* pData, size_t length) {
    if (!pData || length == 0) {
        return 0;
    }
    
    // Debug output
    printf("[u_port_web] Writing %zu bytes\n", length);
    
    // Call JavaScript function to write to Web Serial
    int result = js_serial_write((const char*)pData, length);
    
    if (result < 0) {
        printf("[u_port_web] Write error\n");
        return -1;
    }
    
    return result;
}

/**
 * Read data from UART (Web Serial)
 * 
 * Reads from a buffer that is populated asynchronously by
 * JavaScript code listening to the Web Serial port.
 * Uses emscripten_sleep to yield to browser event loop.
 */
int32_t uPortUartRead(void* handle, void* pData, size_t length, int32_t timeoutMs) {
    if (!pData || length == 0) {
        return 0;
    }
    
    // Implement timeout by polling with sleep to yield to browser
    int32_t startTime = uPortGetTickTimeMs();
    int result = 0;
    
    printf("[u_port_web] 📖 READ REQUESTED: length=%zu, timeout=%dms\n", length, timeoutMs);
    int available_at_start = js_serial_available();
    printf("[u_port_web]   Buffer has %d bytes available at start\n", available_at_start);
    
    while (1) {
        // Check how many bytes are available
        int available = js_serial_available();
        
        // Try to read data
        result = js_serial_read((char*)pData, length);
        
        if (result > 0) {
            printf("[u_port_web] ✅ Read %d bytes (requested %zu, available was %d)\n", result, length, available);
            // Print hex dump for debugging
            printf("[u_port_web]   Hex: ");
            for (int i = 0; i < result && i < 32; i++) {
                printf("%02X ", (unsigned char)((char*)pData)[i]);
            }
            printf("\n");
            // Print ASCII (printable chars only)
            printf("[u_port_web]   ASCII: ");
            for (int i = 0; i < result && i < 32; i++) {
                char c = ((char*)pData)[i];
                printf("%c", (c >= 32 && c < 127) ? c : '.');
            }
            printf("\n");
            break;
        }
        
        // Check timeout
        int32_t elapsed = uPortGetTickTimeMs() - startTime;
        if (elapsed >= timeoutMs) {
            // Timeout - return 0 bytes read
            int final_available = js_serial_available();
            printf("[u_port_web] ⏰ Read timeout after %d ms\n", elapsed);
            printf("[u_port_web]   ❌ TIMEOUT: requested=%zu, available_at_start=%d, available_now=%d\n", 
                   length, available_at_start, final_available);
            if (final_available > 0) {
                printf("[u_port_web]   ⚠️  WARNING: %d bytes ARE available but not read!\n", final_available);
            }
            break;
        }
        
        // Sleep briefly to yield to browser event loop (ASYNCIFY enabled)
        // This allows JavaScript to process incoming serial data
        emscripten_sleep(10);
    }
    
    return result;
}

/**
 * Get number of bytes available in receive buffer
 */
int32_t uPortUartGetReceiveSize(void* handle) {
    return js_serial_available();
}

/**
 * Event send (not used in Web port)
 */
int32_t uPortUartEventSend(void* handle, uint32_t eventBitMap) {
    return 0;
}

/**
 * Event receive (not used in Web port)
 */
int32_t uPortUartEventReceive(void* handle, uint32_t* pEventBitMap) {
    if (pEventBitMap) {
        *pEventBitMap = 0;
    }
    return 0;
}

/**
 * Event send and receive (not used in Web port)
 */
int32_t uPortUartEventSendReceive(void* handle, uint32_t eventSendBitMap,
                                   uint32_t* pEventReceiveBitMap) {
    if (pEventReceiveBitMap) {
        *pEventReceiveBitMap = 0;
    }
    return 0;
}

/**
 * Get event queue handle (not used in Web port)
 */
int32_t uPortUartEventQueueHandle(void* handle) {
    return -1;
}

/* ----------------------------------------------------------------
 * TIME FUNCTIONS
 * -------------------------------------------------------------- */

/**
 * Get system tick count in milliseconds
 * Uses JavaScript Date.now() for timing
 */
EM_JS(int32_t, js_get_time_ms, (), {
    return Date.now() & 0x7FFFFFFF; // Keep within int32_t range
});

int32_t uPortGetTickTimeMs(void) {
    return js_get_time_ms();
}

/* ----------------------------------------------------------------
 * LOGGING FUNCTIONS
 * -------------------------------------------------------------- */

/**
 * Log output function - forwards to JavaScript console
 */
EM_JS(void, js_log, (const char* msg), {
    console.log(UTF8ToString(msg));
});

void uPortLog(const char* pFormat, ...) {
    char buffer[256];
    va_list args;
    va_start(args, pFormat);
    vsnprintf(buffer, sizeof(buffer), pFormat, args);
    va_end(args);
    
    js_log(buffer);
}

/* ----------------------------------------------------------------
 * MUTEX/LOCKING (Simplified for single-threaded JavaScript)
 * -------------------------------------------------------------- */

int32_t uPortMutexCreate(void** ppMutexHandle) {
    // JavaScript is single-threaded, no real mutex needed
    *ppMutexHandle = (void*)1; // Dummy handle
    return 0;
}

int32_t uPortMutexDelete(void* mutexHandle) {
    return 0;
}

int32_t uPortMutexLock(void* mutexHandle) {
    return 0;
}

int32_t uPortMutexUnlock(void* mutexHandle) {
    return 0;
}

int32_t uPortMutexTryLock(void* mutexHandle, int32_t timeoutMs) {
    return 0;
}

/* ----------------------------------------------------------------
 * BACKGROUND RX TASK (Not needed for web - data is buffered in JS)
 * -------------------------------------------------------------- */

#include "u_cx_at_client.h"

/**
 * Create background receive task
 * 
 * In WebAssembly, data reception is handled asynchronously in JavaScript,
 * so no background task is needed.
 */
void uPortBgRxTaskCreate(uCxAtClient_t *pClient) {
    // No background task needed - JavaScript handles async data reception
    printf("[u_port_web] Background RX task creation (no-op)\n");
}

