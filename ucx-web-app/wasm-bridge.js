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
 * @file wasm-bridge.js
 * @brief WASM module loading and typed JavaScript API over ucxclient
 *
 * Loads the Emscripten-generated ucxclient WASM module and exposes
 * a clean async JavaScript API. Wires the serial manager into the
 * WASM module's serialWrite / serialRead / serialAvailable hooks.
 */

class WasmBridge {
    /**
     * @param {SerialManager} serialManager
     */
    constructor(serialManager) {
        this.serial = serialManager;
        this.module = null;
        this.initialized = false;

        /** Callback invoked on each URC from the module */
        this.onURC = null;
    }

    /**
     * Load the Emscripten WASM module and wire up serial I/O.
     * Resolves when the module is ready.
     */
    async load() {
        this.module = await createUCXModule();

        // Wire URC callback
        this.module.onURC = (urc) => {
            if (this.onURC) this.onURC(urc);
        };

        // Wire serial I/O functions called from EM_JS in u_port_web.c
        this.module.serialWrite = (buffer) => this.serial.serialWrite(buffer);
        this.module.serialRead = (maxLen) => this.serial.serialRead(maxLen);
        this.module.serialAvailable = () => this.serial.serialAvailable();
        this.module.serialPort = null; // legacy: some library.js refs check this
    }

    /** Mark the module as having an open serial port (for legacy checks) */
    attachSerialPort() {
        if (this.module) {
            this.module.serialPort = this.serial.port;
        }
    }

    /** Clear the serial port reference */
    detachSerialPort() {
        if (this.module) {
            this.module.serialPort = null;
        }
    }

    // ---- UCX API wrappers ----

    /**
     * Initialise the UCX AT client.
     * @param {number} baudRate
     * @returns {number} 0 on success
     */
    async init(baudRate) {
        const result = await this.module.ccall(
            'ucx_init', 'number', ['string', 'number'], ['', baudRate]);
        if (result === 0) this.initialized = true;
        return result;
    }

    /** Deinitialise UCX client */
    deinit() {
        if (!this.initialized) return;
        this.module.ccall('ucx_deinit', null, [], []);
        this.initialized = false;
    }

    /**
     * Set the WASM-side log level.
     * @param {number} level 0=none, 1=error, 2=info, 3=debug
     */
    setLogLevel(level) {
        this.module.ccall('ucx_set_log_level', null, ['number'], [level]);
    }

    /**
     * Scan for WiFi networks.
     * @returns {Array<{ssid: string, rssi: number, channel: number}>}
     */
    async wifiScan() {
        const scanId = this.module.ccall('ucx_wifi_scan_begin', 'number', [], []);
        if (scanId < 0) throw new Error('Failed to start WiFi scan');

        // Wait for scan responses to arrive
        await new Promise(r => setTimeout(r, 5000));

        const networks = [];
        const ssidPtr = this.module._malloc(33);
        const rssiPtr = this.module._malloc(4);
        const channelPtr = this.module._malloc(4);

        try {
            while (true) {
                const ret = this.module.ccall('ucx_wifi_scan_get_next', 'number',
                    ['number', 'number', 'number'], [ssidPtr, rssiPtr, channelPtr]);
                if (ret !== 1) break;

                const ssid = this.module.UTF8ToString(ssidPtr);
                const rssi = this._readInt32(rssiPtr);
                const channel = this._readInt32(channelPtr);

                networks.push({ ssid, rssi, channel });
            }
        } finally {
            this.module._free(ssidPtr);
            this.module._free(rssiPtr);
            this.module._free(channelPtr);
            this.module.ccall('ucx_wifi_scan_end', null, [], []);
        }

        return networks;
    }

    /**
     * Connect to a WiFi network.
     * @returns {number} 0 on success
     */
    async wifiConnect(ssid, password) {
        return await this.module.ccall(
            'ucx_wifi_connect', 'number',
            ['string', 'string'], [ssid, password || '']);
    }

    /** Disconnect from WiFi. @returns {number} 0 on success */
    async wifiDisconnect() {
        return await this.module.ccall('ucx_wifi_disconnect', 'number', [], []);
    }

    /** Get the current IP address. @returns {string|null} */
    async getIP() {
        const ptr = this.module._malloc(256);
        try {
            const ret = await this.module.ccall(
                'ucx_wifi_get_ip', 'number', ['number'], [ptr]);
            return ret === 0 ? this.module.UTF8ToString(ptr) : null;
        } finally {
            this.module._free(ptr);
        }
    }

    /** Get module firmware version. @returns {string|null} */
    async getVersion() {
        const ptr = this.module._malloc(256);
        try {
            const ret = await this.module.ccall(
                'ucx_get_version', 'number', ['number', 'number'], [ptr, 256]);
            return ret === 0 ? this.module.UTF8ToString(ptr) : null;
        } finally {
            this.module._free(ptr);
        }
    }

    // ---- helpers ----

    _readInt32(ptr) {
        // Call back into C to read the value — avoids all JS typed-array
        // view staleness issues after ASYNCIFY memory growth.
        return this.module.ccall('ucx_read_int32', 'number', ['number'], [ptr]);
    }
}
