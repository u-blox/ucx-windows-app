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
 * @file app.js
 * @brief UCX Web App - UI logic and application state
 *
 * Orchestrates the SerialManager and WasmBridge, handles DOM events,
 * URC processing, and user-facing logging.
 */

// ----------------------------------------------------------------
// GLOBALS
// ----------------------------------------------------------------

let serial = null;   // SerialManager
let bridge = null;   // WasmBridge

// ----------------------------------------------------------------
// LOGGING
// ----------------------------------------------------------------

function log(message) {
    const ts = new Date().toLocaleTimeString();
    const line = `[${ts}] ${message}`;
    console.log(line);
    const el = document.getElementById('log');
    el.textContent += line + '\n';
    el.scrollTop = el.scrollHeight;
}

function clearLog() {
    document.getElementById('log').textContent = '';
}

// ----------------------------------------------------------------
// UI HELPERS
// ----------------------------------------------------------------

function updateSerialStatus(connected) {
    const status  = document.getElementById('serialStatus');
    const ids     = ['disconnectBtn', 'scanBtn', 'connectWifiBtn',
                     'disconnectWifiBtn', 'connectionInfoBtn'];

    if (connected) {
        status.textContent = 'Connected';
        status.className   = 'status connected';
        document.getElementById('connectBtn').disabled = true;
        ids.forEach(id => document.getElementById(id).disabled = false);
    } else {
        status.textContent = 'Not Connected';
        status.className   = 'status disconnected';
        document.getElementById('connectBtn').disabled = false;
        ids.forEach(id => document.getElementById(id).disabled = true);
    }
}

function selectNetwork(ssid) {
    document.getElementById('ssid').value = ssid;
}

// ----------------------------------------------------------------
// SERIAL RX DISPLAY (line-reassembly to handle fragmented chunks)
// ----------------------------------------------------------------

let rxLineBuffer = '';

function onSerialReceive(text) {
    rxLineBuffer += text;

    // Process complete lines (delimited by \r\n or \n)
    let newlineIdx;
    while ((newlineIdx = rxLineBuffer.indexOf('\n')) !== -1) {
        const line = rxLineBuffer.substring(0, newlineIdx).replace(/\r$/, '');
        rxLineBuffer = rxLineBuffer.substring(newlineIdx + 1);

        if (line.length === 0) continue;

        let prefix = 'RX';
        if (line.includes('+UUWLE') || line.includes('+UEWLU')) prefix = 'RX [WIFI-URC]';
        else if (line.includes('OK'))    prefix = 'RX [OK]';
        else if (line.includes('ERROR')) prefix = 'RX [ERROR]';

        log(`${prefix}: ${line}`);
    }
}

// ----------------------------------------------------------------
// URC HANDLER
// ----------------------------------------------------------------

function handleURC(urc) {
    // WiFi Link Event: +UEWLU:<wlan_handle>,<bssid>,<link_up>
    if (urc.includes('+UEWLU:')) {
        const m = urc.match(/\+UEWLU:(\d+),([0-9A-Fa-f:]+),(\d+)/);
        if (m) {
            if (m[3] === '1') {
                log(`WiFi LINK UP (BSSID: ${m[2]})`);
                setTimeout(getConnectionInfo, 2000);
            } else {
                log('WiFi LINK DOWN');
            }
        } else {
            log(`[URC] ${urc}`);
        }
        return;
    }

    // WiFi Station Network Up (DHCP done)
    if (urc.includes('+UEWSNU')) {
        log('WiFi Station Network UP - DHCP completed');
        setTimeout(getConnectionInfo, 500);
        return;
    }

    // WiFi Station Network Down
    if (urc.includes('+UEWSND')) {
        log('WiFi Station Network DOWN');
        return;
    }

    // Legacy WiFi link event
    if (urc.includes('+UUWLE:')) {
        const m = urc.match(/\+UUWLE:(\d+),(\d+)/);
        if (m) {
            log(m[2] === '1' ? 'WiFi CONNECTED' : 'WiFi DISCONNECTED');
            if (m[2] === '1') setTimeout(getConnectionInfo, 1000);
        }
        return;
    }

    // Verbose network events → console only
    if (urc.includes('+UUPSND:') || urc.includes('+UUDPC:')) {
        console.log('[URC]', urc);
        return;
    }

    // Everything else
    log(`[URC] ${urc}`);
}

// ----------------------------------------------------------------
// SERIAL CONNECTION
// ----------------------------------------------------------------

async function connectSerial() {
    try {
        if (!bridge || !bridge.module) {
            alert('WASM module not loaded yet. Please wait.');
            return;
        }
        const baudRate = parseInt(document.getElementById('baudRate').value);

        await serial.connect(baudRate);
        bridge.attachSerialPort();
        log(`Serial port opened at ${baudRate} baud`);

        log('Initializing UCX client...');
        const ret = await bridge.init(baudRate);
        if (ret !== 0) throw new Error(`UCX init failed (${ret})`);
        log('UCX client initialized');

        updateSerialStatus(true);
    } catch (err) {
        log(`Connection error: ${err.message}`);
        alert(`Failed to connect: ${err.message}`);
    }
}

async function disconnectSerial() {
    try {
        bridge.deinit();
        bridge.detachSerialPort();
        await serial.disconnect();
        log('Serial port closed');
        updateSerialStatus(false);
    } catch (err) {
        log(`Disconnect error: ${err.message}`);
    }
}

async function autoConnectSerial() {
    try {
        if (!bridge || !bridge.module) return;
        const baudRate = parseInt(document.getElementById('baudRate').value);

        const ok = await serial.autoConnect(baudRate);
        if (!ok) {
            log('No previously paired ports. Click "Connect to Serial Port".');
            return;
        }

        bridge.attachSerialPort();
        log(`Auto-connected at ${baudRate} baud`);

        log('Initializing UCX client...');
        const ret = await bridge.init(baudRate);
        if (ret !== 0) throw new Error(`UCX init failed (${ret})`);
        log('UCX client initialized');
        updateSerialStatus(true);
    } catch (err) {
        log(`Auto-connect failed: ${err.message}. Click "Connect to Serial Port".`);
    }
}

// ----------------------------------------------------------------
// WIFI
// ----------------------------------------------------------------

async function wifiScan() {
    try {
        log('Starting WiFi scan...');
        const networks = await bridge.wifiScan();
        log(`Found ${networks.length} networks`);

        const div = document.getElementById('scanResults');
        if (networks.length === 0) {
            div.innerHTML = '<p>No networks found</p>';
            return;
        }
        let html = '<table><tr><th>SSID</th><th>Channel</th><th>RSSI</th><th>Action</th></tr>';
        for (const n of networks) {
            html += `<tr><td>${n.ssid}</td><td>${n.channel}</td><td>${n.rssi} dBm</td>`
                  + `<td><button onclick="selectNetwork('${n.ssid}')">Select</button></td></tr>`;
        }
        html += '</table>';
        div.innerHTML = html;
    } catch (err) {
        log(`Scan error: ${err.message}`);
    }
}

async function wifiConnect() {
    try {
        const ssid     = document.getElementById('ssid').value;
        const password = document.getElementById('password').value;
        if (!ssid) { alert('Please enter SSID'); return; }

        log(`Connecting to "${ssid}"...`);
        const startTime = Date.now();
        const ret = await bridge.wifiConnect(ssid, password);
        const elapsed = Date.now() - startTime;

        if (ret === 0) {
            log(`Connect command sent (${elapsed} ms). Waiting for link...`);
            setTimeout(getConnectionInfo, 5000);
        } else {
            log(`WiFi connection failed (error ${ret})`);
        }
    } catch (err) {
        log(`WiFi connect error: ${err.message}`);
    }
}

async function wifiDisconnect() {
    try {
        log('Disconnecting WiFi...');
        const ret = await bridge.wifiDisconnect();
        if (ret === 0) {
            log('WiFi disconnected');
            document.getElementById('connectionInfo').innerHTML = '';
        } else {
            log(`WiFi disconnect failed (error ${ret})`);
        }
    } catch (err) {
        log(`Disconnect error: ${err.message}`);
    }
}

async function getConnectionInfo() {
    try {
        log('Getting connection info...');
        const ip = await bridge.getIP();
        if (ip) {
            log(`IP Address: ${ip}`);
            document.getElementById('connectionInfo').innerHTML =
                `<div style="background:#e6ffe6;padding:10px;border-radius:4px;">` +
                `<strong>Connected!</strong><br>IP Address: <code>${ip}</code></div>`;
        } else {
            log('Failed to get connection info');
        }
    } catch (err) {
        log(`Get info error: ${err.message}`);
    }
}

// ----------------------------------------------------------------
// INITIALISATION
// ----------------------------------------------------------------

async function initApp() {
    serial = new SerialManager();
    serial.onReceive  = onSerialReceive;
    serial.onError    = (msg) => log(`[ERROR] ${msg}`);
    serial.onDisconnect = () => {
        log('Serial port disconnected unexpectedly');
        bridge.deinit();
        bridge.detachSerialPort();
        updateSerialStatus(false);
    };

    bridge = new WasmBridge(serial);
    bridge.onURC = handleURC;

    try {
        await bridge.load();
        log('UCX WASM module loaded');
        document.getElementById('connectBtn').disabled = false;

        // Try auto-connect after a brief delay
        setTimeout(autoConnectSerial, 500);
    } catch (err) {
        log(`Failed to load WASM module: ${err.message}`);
        alert(`Failed to load UCX module: ${err.message}`);
    }
}

// Start when DOM is ready
document.addEventListener('DOMContentLoaded', initApp);
