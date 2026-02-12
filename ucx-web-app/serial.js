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
 * @file serial.js
 * @brief Web Serial API connection management
 *
 * Handles serial port open/close, reading, writing, and disconnect detection.
 * Uses a ring buffer for efficient receive data storage.
 */

// ----------------------------------------------------------------
// RING BUFFER - efficient byte storage for serial receive data
// ----------------------------------------------------------------

class RingBuffer {
    constructor(capacity = 16384) {
        this.buffer = new Uint8Array(capacity);
        this.head = 0;   // write position
        this.tail = 0;   // read position
        this.count = 0;  // bytes stored
        this.capacity = capacity;
    }

    /** Number of bytes available to read */
    get available() {
        return this.count;
    }

    /** Push bytes into the buffer */
    push(data) {
        for (let i = 0; i < data.length; i++) {
            if (this.count >= this.capacity) {
                // Overflow: drop oldest byte
                this.tail = (this.tail + 1) % this.capacity;
                this.count--;
            }
            this.buffer[this.head] = data[i];
            this.head = (this.head + 1) % this.capacity;
            this.count++;
        }
    }

    /** Read up to maxLength bytes, returns Uint8Array or null if empty */
    read(maxLength) {
        if (this.count === 0) {
            return null;
        }
        const length = Math.min(maxLength, this.count);
        const result = new Uint8Array(length);
        for (let i = 0; i < length; i++) {
            result[i] = this.buffer[this.tail];
            this.tail = (this.tail + 1) % this.capacity;
        }
        this.count -= length;
        return result;
    }

    /** Clear all data */
    clear() {
        this.head = 0;
        this.tail = 0;
        this.count = 0;
    }
}

// ----------------------------------------------------------------
// SERIAL MANAGER
// ----------------------------------------------------------------

class SerialManager {
    constructor() {
        this.port = null;
        this.reader = null;
        this.isReading = false;
        this.receiveBuffer = new RingBuffer();
        this.writeQueue = Promise.resolve();

        /** Called when data is received: onReceive(displayText, rawBytes) */
        this.onReceive = null;
        /** Called when the serial port disconnects unexpectedly */
        this.onDisconnect = null;
        /** Called on errors: onError(message) */
        this.onError = null;
    }

    /** Check if Web Serial API is available */
    static isSupported() {
        return 'serial' in navigator;
    }

    /** True when a port is open */
    get connected() {
        return this.port !== null;
    }

    // ---- Connection ----

    /**
     * Try to auto-connect to a previously paired port.
     * @param {number} baudRate
     * @returns {boolean} true if connected
     */
    async autoConnect(baudRate) {
        if (!SerialManager.isSupported()) return false;
        const ports = await navigator.serial.getPorts();
        if (ports.length === 0) return false;

        this.port = ports[0];
        await this.port.open({ baudRate });
        this._startReading();
        this._watchDisconnect();
        return true;
    }

    /**
     * Prompt user to select a serial port and connect.
     * @param {number} baudRate
     */
    async connect(baudRate) {
        if (!SerialManager.isSupported()) {
            throw new Error('Web Serial API not supported. Use Chrome or Edge browser.');
        }
        this.port = await navigator.serial.requestPort();
        await this.port.open({ baudRate });
        this._startReading();
        this._watchDisconnect();
    }

    /** Disconnect and clean up */
    async disconnect() {
        this.isReading = false;
        if (this.reader) {
            await this.reader.cancel();
            this.reader = null;
        }
        if (this.port) {
            await this.port.close();
            this.port = null;
        }
        this.receiveBuffer.clear();
    }

    // ---- Read / Write used by WASM bridge ----

    /** Write raw bytes to the serial port (queued to prevent lock conflicts) */
    serialWrite(buffer) {
        if (!this.port || !this.port.writable) {
            return -1;
        }
        this.writeQueue = this.writeQueue.then(async () => {
            const writer = this.port.writable.getWriter();
            try {
                await writer.write(buffer);
            } catch (err) {
                if (this.onError) this.onError(`Write error: ${err.message}`);
            } finally {
                writer.releaseLock();
            }
        });
        return buffer.length;
    }

    /** Read up to maxLength bytes from the receive ring buffer */
    serialRead(maxLength) {
        return this.receiveBuffer.read(maxLength);
    }

    /** Number of bytes waiting in the receive buffer */
    serialAvailable() {
        return this.receiveBuffer.available;
    }

    // ---- Internal ----

    async _startReading() {
        this.isReading = true;
        try {
            this.reader = this.port.readable.getReader();
            while (this.isReading) {
                const { value, done } = await this.reader.read();
                if (done) break;

                // Store in ring buffer for WASM consumption
                this.receiveBuffer.push(value);

                // Notify listeners for UI display
                if (this.onReceive) {
                    const text = new TextDecoder().decode(value);
                    this.onReceive(text, value);
                }
            }
        } catch (err) {
            if (this.isReading && this.onError) {
                this.onError(`Read error: ${err.message}`);
            }
        } finally {
            if (this.reader) {
                this.reader.releaseLock();
                this.reader = null;
            }
        }
    }

    _watchDisconnect() {
        if (!this.port) return;
        this.port.addEventListener('disconnect', () => {
            this.isReading = false;
            this.port = null;
            if (this.onDisconnect) this.onDisconnect();
        });
    }
}
