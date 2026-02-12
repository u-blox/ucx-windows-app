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

    // ---- BLE Scan ----

    /**
     * Scan for BLE devices.
     * @param {number} timeoutMs  Discovery duration (0 = default ~10 s)
     * @returns {Array<{addr: string, rssi: number, name: string}>}
     */
    async btScan(timeoutMs = 10000) {
        const rc = await this.module.ccall(
            'ucx_bt_discovery_begin', 'number', ['number'], [timeoutMs]);
        if (rc < 0) throw new Error(`BLE scan start failed (${rc})`);

        // Allow the module time to collect responses
        await new Promise(r => setTimeout(r, timeoutMs + 2000));

        const devices = [];
        const addrPtr = this.module._malloc(32);
        const rssiPtr = this.module._malloc(4);
        const namePtr = this.module._malloc(64);

        try {
            while (true) {
                const ret = await this.module.ccall(
                    'ucx_bt_discovery_get_next', 'number',
                    ['number', 'number', 'number'], [addrPtr, rssiPtr, namePtr]);
                if (ret !== 1) break;

                const addr = this.module.UTF8ToString(addrPtr);
                const rssi = this._readInt32(rssiPtr);
                const name = this.module.UTF8ToString(namePtr);
                devices.push({ addr, rssi, name });
            }
        } finally {
            this.module._free(addrPtr);
            this.module._free(rssiPtr);
            this.module._free(namePtr);
            this.module.ccall('ucx_bt_discovery_end', null, [], []);
        }
        return devices;
    }

    // ---- BLE Connect / Disconnect ----

    /**
     * Connect to a BLE peripheral.
     * @param {string} addrStr  BD address string (e.g. "AABBCCDDEEFF" with type suffix)
     * @returns {number} Connection handle (>= 0)
     */
    async btConnect(addrStr) {
        const chPtr = this.module._malloc(4);
        try {
            const rc = await this.module.ccall(
                'ucx_bt_connect', 'number',
                ['string', 'number'], [addrStr, chPtr]);
            if (rc !== 0) throw new Error(`BLE connect failed (${rc})`);
            return this._readInt32(chPtr);
        } finally {
            this.module._free(chPtr);
        }
    }

    /**
     * Disconnect a BLE connection.
     * @param {number} connHandle
     * @returns {number} 0 on success
     */
    async btDisconnect(connHandle) {
        return await this.module.ccall(
            'ucx_bt_disconnect', 'number', ['number'], [connHandle]);
    }

    // ---- GATT Client ----

    /**
     * Discover primary services on a connected BLE peer.
     * @param {number} connHandle
     * @returns {Array<{startHandle: number, endHandle: number, uuid: string}>}
     */
    async gattDiscoverServices(connHandle) {
        const rc = await this.module.ccall(
            'ucx_gatt_discover_services_begin', 'number',
            ['number'], [connHandle]);
        if (rc < 0) throw new Error(`Service discovery failed (${rc})`);

        const services = [];
        const shPtr = this.module._malloc(4);
        const ehPtr = this.module._malloc(4);
        const uuidPtr = this.module._malloc(40);

        try {
            while (true) {
                const ret = await this.module.ccall(
                    'ucx_gatt_discover_services_get_next', 'number',
                    ['number', 'number', 'number'], [shPtr, ehPtr, uuidPtr]);
                if (ret !== 1) break;

                services.push({
                    startHandle: this._readInt32(shPtr),
                    endHandle:   this._readInt32(ehPtr),
                    uuid:        this.module.UTF8ToString(uuidPtr)
                });
            }
        } finally {
            this.module._free(shPtr);
            this.module._free(ehPtr);
            this.module._free(uuidPtr);
            this.module.ccall('ucx_gatt_discover_services_end', null, [], []);
        }
        return services;
    }

    /**
     * Discover characteristics of a service.
     * @param {number} connHandle
     * @param {number} startHandle
     * @param {number} endHandle
     * @returns {Array<{attrHandle, valueHandle, properties, uuid}>}
     */
    async gattDiscoverChars(connHandle, startHandle, endHandle) {
        const rc = await this.module.ccall(
            'ucx_gatt_discover_chars_begin', 'number',
            ['number', 'number', 'number'], [connHandle, startHandle, endHandle]);
        if (rc < 0) throw new Error(`Char discovery failed (${rc})`);

        const chars = [];
        const ahPtr = this.module._malloc(4);
        const vhPtr = this.module._malloc(4);
        const prPtr = this.module._malloc(4);
        const uuidPtr = this.module._malloc(40);

        try {
            while (true) {
                const ret = await this.module.ccall(
                    'ucx_gatt_discover_chars_get_next', 'number',
                    ['number', 'number', 'number', 'number'],
                    [ahPtr, vhPtr, prPtr, uuidPtr]);
                if (ret !== 1) break;

                chars.push({
                    attrHandle:  this._readInt32(ahPtr),
                    valueHandle: this._readInt32(vhPtr),
                    properties:  this._readInt32(prPtr),
                    uuid:        this.module.UTF8ToString(uuidPtr)
                });
            }
        } finally {
            this.module._free(ahPtr);
            this.module._free(vhPtr);
            this.module._free(prPtr);
            this.module._free(uuidPtr);
            this.module.ccall('ucx_gatt_discover_chars_end', null, [], []);
        }
        return chars;
    }

    /**
     * Read a GATT characteristic.
     * @param {number} connHandle
     * @param {number} valueHandle
     * @returns {Uint8Array} read data
     */
    async gattRead(connHandle, valueHandle) {
        const bufSize = 256;
        const bufPtr = this.module._malloc(bufSize);
        const copyPtr = this.module._malloc(bufSize);
        try {
            const len = await this.module.ccall(
                'ucx_gatt_read', 'number',
                ['number', 'number', 'number', 'number'],
                [connHandle, valueHandle, bufPtr, bufSize]);
            if (len < 0) throw new Error(`GATT read failed (${len})`);
            // Use C-side copy to avoid stale HEAPU8 after ASYNCIFY
            this.module.ccall('ucx_copy_bytes', null,
                ['number', 'number', 'number'], [bufPtr, copyPtr, len]);
            const result = new Uint8Array(len);
            for (let i = 0; i < len; i++) {
                result[i] = this.module.HEAPU8[copyPtr + i];
            }
            return result;
        } finally {
            this.module._free(bufPtr);
            this.module._free(copyPtr);
        }
    }

    /**
     * Write a GATT characteristic (with response).
     * @param {number} connHandle
     * @param {number} valueHandle
     * @param {Uint8Array|Array<number>} data
     * @returns {number} 0 on success
     */
    async gattWrite(connHandle, valueHandle, data) {
        const ptr = this.module._malloc(data.length);
        try {
            this.module.HEAPU8.set(data, ptr);
            return await this.module.ccall(
                'ucx_gatt_write', 'number',
                ['number', 'number', 'number', 'number'],
                [connHandle, valueHandle, ptr, data.length]);
        } finally {
            this.module._free(ptr);
        }
    }

    /**
     * Enable / disable notifications or indications on a CCCD.
     * @param {number} connHandle
     * @param {number} cccdHandle
     * @param {number} config  0=none, 1=notifications, 2=indications, 3=both
     * @returns {number} 0 on success
     */
    async gattConfigWrite(connHandle, cccdHandle, config) {
        return await this.module.ccall(
            'ucx_gatt_config_write', 'number',
            ['number', 'number', 'number'],
            [connHandle, cccdHandle, config]);
    }

    // ---- GATT Server ----

    /**
     * Define a GATT service.
     * @param {Uint8Array|Array<number>} uuidBytes  (2 bytes for 16-bit, 16 for 128-bit)
     * @returns {number} service handle
     */
    async gattServerDefineService(uuidBytes) {
        const uPtr = this.module._malloc(uuidBytes.length);
        const shPtr = this.module._malloc(4);
        try {
            this.module.HEAPU8.set(uuidBytes, uPtr);
            const rc = await this.module.ccall(
                'ucx_gatt_server_service_define', 'number',
                ['number', 'number', 'number'],
                [uPtr, uuidBytes.length, shPtr]);
            if (rc !== 0) throw new Error(`Service define failed (${rc})`);
            return this._readInt32(shPtr);
        } finally {
            this.module._free(uPtr);
            this.module._free(shPtr);
        }
    }

    /**
     * Define a GATT server characteristic.
     * @param {Uint8Array|Array<number>} uuidBytes
     * @param {number} properties  bitmask (0x02=read, 0x08=write, 0x10=notify …)
     * @param {Uint8Array|Array<number>|null} initialValue
     * @returns {{valueHandle: number, cccdHandle: number}}
     */
    async gattServerDefineChar(uuidBytes, properties, initialValue) {
        const uPtr = this.module._malloc(uuidBytes.length);
        const vhPtr = this.module._malloc(4);
        const chPtr = this.module._malloc(4);

        let ivPtr = 0;
        let ivLen = 0;
        if (initialValue && initialValue.length > 0) {
            ivPtr = this.module._malloc(initialValue.length);
            this.module.HEAPU8.set(initialValue, ivPtr);
            ivLen = initialValue.length;
        }

        try {
            this.module.HEAPU8.set(uuidBytes, uPtr);
            const rc = await this.module.ccall(
                'ucx_gatt_server_char_define', 'number',
                ['number', 'number', 'number', 'number', 'number', 'number', 'number'],
                [uPtr, uuidBytes.length, properties, ivPtr, ivLen, vhPtr, chPtr]);
            if (rc !== 0) throw new Error(`Char define failed (${rc})`);
            return {
                valueHandle: this._readInt32(vhPtr),
                cccdHandle:  this._readInt32(chPtr)
            };
        } finally {
            this.module._free(uPtr);
            this.module._free(vhPtr);
            this.module._free(chPtr);
            if (ivPtr) this.module._free(ivPtr);
        }
    }

    /** Activate the GATT server. @returns {number} 0 on success */
    async gattServerActivate() {
        return await this.module.ccall(
            'ucx_gatt_server_activate', 'number', [], []);
    }

    /**
     * Set a GATT server attribute value.
     * @param {number} attrHandle
     * @param {Uint8Array|Array<number>} value
     * @returns {number} 0 on success
     */
    async gattServerSetValue(attrHandle, value) {
        const ptr = this.module._malloc(value.length);
        try {
            this.module.HEAPU8.set(value, ptr);
            return await this.module.ccall(
                'ucx_gatt_server_set_value', 'number',
                ['number', 'number', 'number'],
                [attrHandle, ptr, value.length]);
        } finally {
            this.module._free(ptr);
        }
    }

    /**
     * Send a GATT notification.
     * @param {number} connHandle
     * @param {number} charHandle
     * @param {Uint8Array|Array<number>} value
     * @returns {number} 0 on success
     */
    async gattServerNotify(connHandle, charHandle, value) {
        const ptr = this.module._malloc(value.length);
        try {
            this.module.HEAPU8.set(value, ptr);
            return await this.module.ccall(
                'ucx_gatt_server_send_notification', 'number',
                ['number', 'number', 'number', 'number'],
                [connHandle, charHandle, ptr, value.length]);
        } finally {
            this.module._free(ptr);
        }
    }

    /** Start BLE advertising. @returns {number} 0 on success */
    async btAdvertiseStart() {
        return await this.module.ccall(
            'ucx_bt_advertise_start', 'number', [], []);
    }

    /** Stop BLE advertising. @returns {number} 0 on success */
    async btAdvertiseStop() {
        return await this.module.ccall(
            'ucx_bt_advertise_stop', 'number', [], []);
    }

    // ---- BLE Device Name ----

    /**
     * Set the local BLE device name.
     * @param {string} name  Device name (max ~29 chars)
     * @returns {number} 0 on success
     */
    async btSetLocalName(name) {
        return await this.module.ccall(
            'ucx_bt_set_local_name', 'number', ['string'], [name]);
    }

    /**
     * Get the current BLE device name.
     * @returns {string|null}
     */
    async btGetLocalName() {
        const ptr = this.module._malloc(64);
        try {
            const rc = await this.module.ccall(
                'ucx_bt_get_local_name', 'number',
                ['number', 'number'], [ptr, 64]);
            return rc === 0 ? this.module.UTF8ToString(ptr) : null;
        } finally {
            this.module._free(ptr);
        }
    }

    // ---- Pairing / Bonding ----

    /**
     * Set BLE pairing mode.
     * @param {number} mode  0 = disabled, 1 = enabled
     * @returns {number} 0 on success
     */
    async btSetPairingMode(mode) {
        return await this.module.ccall(
            'ucx_bt_set_pairing_mode', 'number', ['number'], [mode]);
    }

    /**
     * Set BLE I/O capabilities for pairing.
     * @param {number} ioCap  0=NoIO, 1=DisplayOnly, 2=DisplayYesNo, 3=KeyboardOnly, 4=KeyboardDisplay
     * @returns {number} 0 on success
     */
    async btSetIoCapabilities(ioCap) {
        return await this.module.ccall(
            'ucx_bt_set_io_capabilities', 'number', ['number'], [ioCap]);
    }

    /**
     * Respond to a user-confirmation pairing request.
     * @param {string} addrStr  BD address of requesting peer
     * @param {boolean} accept  true to accept, false to deny
     * @returns {number} 0 on success
     */
    async btUserConfirmation(addrStr, accept) {
        return await this.module.ccall(
            'ucx_bt_user_confirmation', 'number',
            ['string', 'number'], [addrStr, accept ? 1 : 0]);
    }

    // ---- GATT Client Descriptor Discovery ----

    /**
     * Discover descriptors of a characteristic (finds the real CCCD handle).
     * @param {number} connHandle
     * @param {number} valueHandle  Characteristic value handle
     * @param {number} charEnd      End handle of the characteristic range
     * @returns {Array<{charHandle: number, descHandle: number, uuid: string}>}
     */
    async gattDiscoverDescriptors(connHandle, valueHandle, charEnd) {
        const rc = await this.module.ccall(
            'ucx_gatt_discover_descriptors_begin', 'number',
            ['number', 'number', 'number'],
            [connHandle, valueHandle, charEnd]);
        if (rc < 0) throw new Error(`Descriptor discovery failed (${rc})`);

        const descs = [];
        const chPtr = this.module._malloc(4);
        const dhPtr = this.module._malloc(4);
        const uuidPtr = this.module._malloc(40);

        try {
            while (true) {
                const ret = await this.module.ccall(
                    'ucx_gatt_discover_descriptors_get_next', 'number',
                    ['number', 'number', 'number'], [chPtr, dhPtr, uuidPtr]);
                if (ret !== 1) break;

                descs.push({
                    charHandle: this._readInt32(chPtr),
                    descHandle: this._readInt32(dhPtr),
                    uuid:       this.module.UTF8ToString(uuidPtr)
                });
            }
        } finally {
            this.module._free(chPtr);
            this.module._free(dhPtr);
            this.module._free(uuidPtr);
            this.module.ccall('ucx_gatt_discover_descriptors_end', null, [], []);
        }
        return descs;
    }

    // ---- helpers ----

    _readInt32(ptr) {
        // Call back into C to read the value — avoids all JS typed-array
        // view staleness issues after ASYNCIFY memory growth.
        return this.module.ccall('ucx_read_int32', 'number', ['number'], [ptr]);
    }
}
