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
        const buffer = new Uint8Array(Module.HEAPU8.buffer, data, length);
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
            Module.HEAPU8.set(jsBuffer, buffer);
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
int32_t uPortUartOpen(int32_t uart, int32_t baudRate, void* pReceiveBuffer,
                      size_t receiveBufferSizeBytes,
                      int32_t pinTx, int32_t pinRx,
                      int32_t pinCts, int32_t pinRts) {
    // Web Serial port is already opened in JavaScript
    // Just log the configuration for debugging
    printf("[u_port_web] UART open: uart=%d, baudRate=%d\n", uart, baudRate);
    return 0; // Success
}

/**
 * Close a UART device (Web Serial port)
 * 
 * Cleanup is handled in JavaScript when the page unloads,
 * so this is mostly a no-op.
 */
void uPortUartClose(int32_t uart) {
    printf("[u_port_web] UART close: uart=%d\n", uart);
    // Actual port closing is handled in JavaScript
}

/**
 * Write data to UART (Web Serial)
 * 
 * Sends data through the Web Serial API by calling the
 * JavaScript bridge function.
 */
int32_t uPortUartWrite(int32_t uart, const void* pBuffer, size_t sizeBytes) {
    if (!pBuffer || sizeBytes == 0) {
        return 0;
    }
    
    // Debug output
    printf("[u_port_web] Writing %zu bytes\n", sizeBytes);
    
    // Call JavaScript function to write to Web Serial
    int result = js_serial_write((const char*)pBuffer, sizeBytes);
    
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
 */
int32_t uPortUartRead(int32_t uart, void* pBuffer, size_t sizeBytes) {
    if (!pBuffer || sizeBytes == 0) {
        return 0;
    }
    
    // Call JavaScript function to read from receive buffer
    int result = js_serial_read((char*)pBuffer, sizeBytes);
    
    if (result > 0) {
        printf("[u_port_web] Read %d bytes\n", result);
    }
    
    return result;
}

/**
 * Get number of bytes available in receive buffer
 */
int32_t uPortUartGetReceiveSize(int32_t uart) {
    return js_serial_available();
}

/**
 * Event send (not used in Web port)
 */
int32_t uPortUartEventSend(int32_t uart, uint32_t eventBitMap) {
    return 0;
}

/**
 * Event receive (not used in Web port)
 */
int32_t uPortUartEventReceive(int32_t uart, uint32_t* pEventBitMap) {
    if (pEventBitMap) {
        *pEventBitMap = 0;
    }
    return 0;
}

/**
 * Event send and receive (not used in Web port)
 */
int32_t uPortUartEventSendReceive(int32_t uart, uint32_t eventSendBitMap,
                                   uint32_t* pEventReceiveBitMap) {
    if (pEventReceiveBitMap) {
        *pEventReceiveBitMap = 0;
    }
    return 0;
}

/**
 * Get event queue handle (not used in Web port)
 */
int32_t uPortUartEventQueueHandle(int32_t uart) {
    return -1;
}

/* ----------------------------------------------------------------
 * TIME FUNCTIONS
 * -------------------------------------------------------------- */

/**
 * Get system tick count in milliseconds
 * Uses JavaScript Date.now() for timing
 */
EM_JS(int64_t, js_get_time_ms, (), {
    return Date.now();
});

int64_t uPortGetTickTimeMs(void) {
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
 * INITIALIZATION
 * -------------------------------------------------------------- */

/**
 * Initialize the port layer
 * Called once when the WASM module loads
 */
int32_t uPortInit(void) {
    printf("[u_port_web] Port layer initialized\n");
    return 0;
}

/**
 * Deinitialize the port layer
 */
void uPortDeinit(void) {
    printf("[u_port_web] Port layer deinitialized\n");
}
