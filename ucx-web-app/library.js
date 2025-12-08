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
 * @file library.js
 * @brief JavaScript library for WebAssembly UCX client
 * 
 * This file is merged into the WASM module at compile time by Emscripten.
 * It provides JavaScript implementations of functions that are called from C code.
 */

mergeInto(LibraryManager.library, {
    /**
     * Write data to Web Serial port
     * @param {Uint8Array} buffer - Data to write
     * @returns {number} Number of bytes written, or -1 on error
     */
    serialWrite: function(buffer) {
        try {
            if (!Module.serialPort || !Module.serialPort.writable) {
                console.error('[library.js] Serial port not writable');
                return -1;
            }
            
            const writer = Module.serialPort.writable.getWriter();
            
            // Write the data
            writer.write(buffer).then(() => {
                console.log('[library.js] Wrote', buffer.length, 'bytes');
            }).catch(error => {
                console.error('[library.js] Write error:', error);
            }).finally(() => {
                writer.releaseLock();
            });
            
            return buffer.length;
        } catch (error) {
            console.error('[library.js] serialWrite exception:', error);
            return -1;
        }
    },
    
    /**
     * Read data from receive buffer
     * @param {number} maxLength - Maximum bytes to read
     * @returns {Uint8Array|null} Data read, or null if no data
     */
    serialRead: function(maxLength) {
        try {
            if (!Module.receiveBuffer || Module.receiveBuffer.length === 0) {
                return null;
            }
            
            // Read up to maxLength bytes from buffer
            const length = Math.min(maxLength, Module.receiveBuffer.length);
            const data = Module.receiveBuffer.slice(0, length);
            
            // Remove read data from buffer
            Module.receiveBuffer = Module.receiveBuffer.slice(length);
            
            console.log('[library.js] Read', length, 'bytes from buffer');
            return new Uint8Array(data);
        } catch (error) {
            console.error('[library.js] serialRead exception:', error);
            return null;
        }
    },
    
    /**
     * Get number of bytes available in receive buffer
     * @returns {number} Number of bytes available
     */
    serialAvailable: function() {
        try {
            if (!Module.receiveBuffer) {
                return 0;
            }
            return Module.receiveBuffer.length;
        } catch (error) {
            console.error('[library.js] serialAvailable exception:', error);
            return 0;
        }
    },
    
    /**
     * Log message to console
     * @param {string} message - Message to log
     */
    log: function(msgPtr) {
        const message = UTF8ToString(msgPtr);
        console.log('[UCX-WASM]', message);
    },
    
    /**
     * URC callback - called when unsolicited result code is received
     * @param {string} urcLine - URC line
     */
    urcCallback: function(urcPtr) {
        const urc = UTF8ToString(urcPtr);
        console.log('[URC]', urc);
        
        // Forward to user-defined handler if available
        if (Module.onURC) {
            Module.onURC(urc);
        }
    }
});
