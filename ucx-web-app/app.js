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
    const ts = new Date().toLocaleTimeString('en-GB', { hour12: false });
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
                     'disconnectWifiBtn', 'connectionInfoBtn',
                     'btScanBtn', 'btConnectBtn', 'btDisconnectBtn',
                     'gattDiscoverBtn', 'gattReadBtn', 'gattWriteBtn',
                     'gattNotifyBtn',
                     'gattServerDefineBtn', 'gattServerActivateBtn',
                     'gattServerSetValueBtn', 'gattServerNotifyBtn',
                     'btAdvStartBtn', 'btAdvStopBtn'];

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

    // BLE connection event: +UEBTC:<conn_handle>,<bd_addr>
    if (urc.includes('+UEBTC:')) {
        const m = urc.match(/\+UEBTC:(\d+),(.+)/);
        if (m) {
            log(`BLE CONNECTED (handle=${m[1]}, addr=${m[2]})`);
            document.getElementById('btConnHandle').value = m[1];
        } else {
            log(`[URC] ${urc}`);
        }
        return;
    }

    // BLE disconnection event: +UEBTDC:<conn_handle>
    if (urc.includes('+UEBTDC:')) {
        const m = urc.match(/\+UEBTDC:(\d+)/);
        if (m) {
            log(`BLE DISCONNECTED (handle=${m[1]})`);
        } else {
            log(`[URC] ${urc}`);
        }
        return;
    }

    // GATT client notification: +UEBTGCN:<conn_handle>,<char_handle>,<hex_data>
    if (urc.includes('+UEBTGCN:')) {
        const m = urc.match(/\+UEBTGCN:(\d+),(\d+),(.+)/);
        if (m) {
            log(`GATT NOTIFICATION (conn=${m[1]}, char=${m[2]}): ${m[3]}`);
        } else {
            log(`[URC] ${urc}`);
        }
        return;
    }

    // GATT server write from peer: +UEBTGSW:<conn_handle>,<attr_handle>,<hex_data>
    if (urc.includes('+UEBTGSW:')) {
        const m = urc.match(/\+UEBTGSW:(\d+),(\d+),(.+)/);
        if (m) {
            log(`GATT SERVER WRITE (conn=${m[1]}, attr=${m[2]}): ${m[3]}`);
        } else {
            log(`[URC] ${urc}`);
        }
        return;
    }

    // Background BLE discovery events: +UEBTBGD:<addr>,<rssi>,<name>,<data_type>,<data>
    if (urc.includes('+UEBTBGD:')) {
        log(`[BT-DISCOVERY] ${urc}`);
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
// BLUETOOTH SCAN
// ----------------------------------------------------------------

async function btScan() {
    try {
        const timeout = parseInt(document.getElementById('btScanTimeout').value) || 10000;
        log(`Starting BLE scan (${timeout} ms)...`);
        const devices = await bridge.btScan(timeout);
        log(`Found ${devices.length} BLE devices`);

        const div = document.getElementById('btScanResults');
        if (devices.length === 0) {
            div.innerHTML = '<p>No BLE devices found</p>';
            return;
        }
        let html = '<table><tr><th>Name</th><th>Address</th><th>RSSI</th><th>Action</th></tr>';
        for (const d of devices) {
            const displayName = d.name || '(unknown)';
            html += `<tr><td>${displayName}</td><td style="font-family:monospace">${d.addr}</td>`
                  + `<td>${d.rssi} dBm</td>`
                  + `<td><button onclick="selectBtDevice('${d.addr}')">Select</button></td></tr>`;
        }
        html += '</table>';
        div.innerHTML = html;
    } catch (err) {
        log(`BLE scan error: ${err.message}`);
    }
}

function selectBtDevice(addr) {
    document.getElementById('btAddr').value = addr;
}

async function btConnect() {
    try {
        const addr = document.getElementById('btAddr').value.trim();
        if (!addr) { alert('Please enter or select a BLE address'); return; }

        log(`Connecting to BLE device ${addr}...`);
        const connHandle = await bridge.btConnect(addr);
        log(`BLE connected, handle=${connHandle}`);
        document.getElementById('btConnHandle').value = connHandle;
    } catch (err) {
        log(`BLE connect error: ${err.message}`);
    }
}

async function btDisconnect() {
    try {
        const ch = parseInt(document.getElementById('btConnHandle').value);
        if (isNaN(ch)) { alert('No connection handle'); return; }

        log(`Disconnecting BLE handle=${ch}...`);
        const rc = await bridge.btDisconnect(ch);
        if (rc === 0) {
            log('BLE disconnected');
            document.getElementById('btConnHandle').value = '';
        } else {
            log(`BLE disconnect failed (${rc})`);
        }
    } catch (err) {
        log(`BLE disconnect error: ${err.message}`);
    }
}

// ----------------------------------------------------------------
// GATT CLIENT
// ----------------------------------------------------------------

async function gattDiscover() {
    try {
        const ch = parseInt(document.getElementById('btConnHandle').value);
        if (isNaN(ch)) { alert('Connect to a BLE device first'); return; }

        log('Discovering GATT services...');
        const services = await bridge.gattDiscoverServices(ch);
        log(`Found ${services.length} services`);

        const div = document.getElementById('gattServices');
        if (services.length === 0) {
            div.innerHTML = '<p>No services found</p>';
            return;
        }

        let html = '';
        for (const svc of services) {
            html += `<div style="border:1px solid #ddd;padding:10px;margin:5px 0;border-radius:4px;">`;
            html += `<strong>Service UUID:</strong> <code>${svc.uuid}</code>`;
            html += ` (handles ${svc.startHandle}–${svc.endHandle})`;
            html += ` <button onclick="gattDiscoverChars(${ch},${svc.startHandle},${svc.endHandle})">Discover Chars</button>`;
            html += `<div id="svc_${svc.startHandle}_chars"></div>`;
            html += `</div>`;
        }
        div.innerHTML = html;
    } catch (err) {
        log(`GATT discover error: ${err.message}`);
    }
}

async function gattDiscoverChars(connHandle, startHandle, endHandle) {
    try {
        log(`Discovering chars for service ${startHandle}–${endHandle}...`);
        const chars = await bridge.gattDiscoverChars(connHandle, startHandle, endHandle);
        log(`Found ${chars.length} characteristics`);

        const div = document.getElementById(`svc_${startHandle}_chars`);
        if (chars.length === 0) {
            div.innerHTML = '<p>No characteristics</p>';
            return;
        }

        let html = '<table><tr><th>UUID</th><th>Value Handle</th><th>Properties</th><th>Actions</th></tr>';
        for (const c of chars) {
            const props = formatBleProperties(c.properties);
            let actions = '';
            if (c.properties & 0x02) {
                actions += `<button onclick="gattReadChar(${connHandle},${c.valueHandle})">Read</button> `;
            }
            if (c.properties & 0x08 || c.properties & 0x04) {
                actions += `<button onclick="promptGattWrite(${connHandle},${c.valueHandle})">Write</button> `;
            }
            if (c.properties & 0x10) {
                actions += `<button onclick="gattEnableNotify(${connHandle},${c.valueHandle + 1},1)">Notify ON</button> `;
            }
            if (c.properties & 0x20) {
                actions += `<button onclick="gattEnableNotify(${connHandle},${c.valueHandle + 1},2)">Indicate ON</button> `;
            }
            html += `<tr><td><code>${c.uuid}</code></td><td>${c.valueHandle}</td>`
                  + `<td>${props}</td><td>${actions}</td></tr>`;
        }
        html += '</table>';
        div.innerHTML = html;
    } catch (err) {
        log(`Char discover error: ${err.message}`);
    }
}

function formatBleProperties(p) {
    const flags = [];
    if (p & 0x01) flags.push('Broadcast');
    if (p & 0x02) flags.push('Read');
    if (p & 0x04) flags.push('WriteNoResp');
    if (p & 0x08) flags.push('Write');
    if (p & 0x10) flags.push('Notify');
    if (p & 0x20) flags.push('Indicate');
    return flags.join(', ') || `0x${p.toString(16)}`;
}

async function gattReadChar(connHandle, valueHandle) {
    try {
        log(`GATT read (conn=${connHandle}, val=${valueHandle})...`);
        const data = await bridge.gattRead(connHandle, valueHandle);
        const hex = Array.from(data).map(b => b.toString(16).padStart(2, '0')).join(' ');

        // Try to decode as UTF-8 text
        let text = '';
        try { text = new TextDecoder().decode(data); } catch (e) { /* ignore */ }
        const readable = /^[\x20-\x7e]+$/.test(text) ? ` ("${text}")` : '';

        log(`GATT read result [${data.length} bytes]: ${hex}${readable}`);
    } catch (err) {
        log(`GATT read error: ${err.message}`);
    }
}

function promptGattWrite(connHandle, valueHandle) {
    const input = prompt('Enter hex bytes to write (e.g. "01 02 FF"):');
    if (!input) return;
    const bytes = input.trim().split(/\s+/).map(s => parseInt(s, 16));
    if (bytes.some(isNaN)) { alert('Invalid hex input'); return; }
    gattWriteChar(connHandle, valueHandle, new Uint8Array(bytes));
}

async function gattWriteChar(connHandle, valueHandle, data) {
    try {
        const hex = Array.from(data).map(b => b.toString(16).padStart(2, '0')).join(' ');
        log(`GATT write (conn=${connHandle}, val=${valueHandle}): ${hex}`);
        const rc = await bridge.gattWrite(connHandle, valueHandle, data);
        if (rc === 0) {
            log('GATT write OK');
        } else {
            log(`GATT write failed (${rc})`);
        }
    } catch (err) {
        log(`GATT write error: ${err.message}`);
    }
}

async function gattEnableNotify(connHandle, cccdHandle, config) {
    try {
        const label = config === 1 ? 'notifications' : config === 2 ? 'indications' : 'notify+indicate';
        log(`Enabling ${label} (conn=${connHandle}, cccd=${cccdHandle})...`);
        const rc = await bridge.gattConfigWrite(connHandle, cccdHandle, config);
        if (rc === 0) {
            log(`${label} enabled`);
        } else {
            log(`Config write failed (${rc})`);
        }
    } catch (err) {
        log(`Config write error: ${err.message}`);
    }
}

// ----------------------------------------------------------------
// GATT SERVER
// ----------------------------------------------------------------

/** Parse a UUID hex string (e.g. "2A00" or "12345678-1234-…") into bytes */
function parseUuidHex(str) {
    const hex = str.replace(/-/g, '').trim();
    const bytes = [];
    for (let i = 0; i < hex.length; i += 2) {
        bytes.push(parseInt(hex.substring(i, i + 2), 16));
    }
    return new Uint8Array(bytes);
}

let gattServerState = {
    serviceHandle: null,
    chars: []                // {name, valueHandle, cccdHandle, properties}
};

async function gattServerDefine() {
    try {
        const svcUuid = document.getElementById('gattSrvUuid').value.trim();
        if (!svcUuid) { alert('Enter a service UUID'); return; }

        const svcBytes = parseUuidHex(svcUuid);
        log(`Defining GATT service (UUID=${svcUuid}, ${svcBytes.length} bytes)...`);
        const svcHandle = await bridge.gattServerDefineService(svcBytes);
        gattServerState.serviceHandle = svcHandle;
        log(`Service defined (handle=${svcHandle})`);

        // Define a read+write+notify characteristic
        const charUuid = document.getElementById('gattSrvCharUuid').value.trim();
        if (!charUuid) { alert('Enter a characteristic UUID'); return; }

        const charBytes = parseUuidHex(charUuid);
        const propsStr = document.getElementById('gattSrvCharProps').value;
        const props = parseInt(propsStr, 16) || 0x1A; // default: Read+Write+Notify

        const initialVal = new TextEncoder().encode('Hello');
        log(`Defining characteristic (UUID=${charUuid}, props=0x${props.toString(16)})...`);
        const charResult = await bridge.gattServerDefineChar(charBytes, props, initialVal);
        gattServerState.chars.push({
            name: charUuid,
            valueHandle: charResult.valueHandle,
            cccdHandle: charResult.cccdHandle,
            properties: props
        });
        log(`Char defined (valueHandle=${charResult.valueHandle}, cccdHandle=${charResult.cccdHandle})`);

        document.getElementById('gattServerInfo').innerHTML =
            `<p style="color:green">Service handle: ${svcHandle}<br>`
            + `Char value handle: ${charResult.valueHandle}, CCCD handle: ${charResult.cccdHandle}</p>`;
    } catch (err) {
        log(`GATT server define error: ${err.message}`);
    }
}

async function gattServerActivate() {
    try {
        log('Activating GATT server...');
        const rc = await bridge.gattServerActivate();
        if (rc === 0) {
            log('GATT server activated');
        } else {
            log(`GATT server activate failed (${rc})`);
        }
    } catch (err) {
        log(`GATT server activate error: ${err.message}`);
    }
}

async function gattServerSetValue() {
    try {
        if (gattServerState.chars.length === 0) {
            alert('Define a characteristic first');
            return;
        }
        const valStr = document.getElementById('gattSrvValue').value;
        const data = new TextEncoder().encode(valStr);
        const vh = gattServerState.chars[0].valueHandle;

        log(`Setting attr value (handle=${vh}, "${valStr}")...`);
        const rc = await bridge.gattServerSetValue(vh, data);
        if (rc === 0) {
            log('Attribute value set');
        } else {
            log(`Set value failed (${rc})`);
        }
    } catch (err) {
        log(`Set value error: ${err.message}`);
    }
}

async function gattServerNotify() {
    try {
        if (gattServerState.chars.length === 0) {
            alert('Define a characteristic first');
            return;
        }
        const ch = parseInt(document.getElementById('btConnHandle').value);
        if (isNaN(ch)) { alert('No active connection'); return; }

        const valStr = document.getElementById('gattSrvValue').value;
        const data = new TextEncoder().encode(valStr);
        const charHandle = gattServerState.chars[0].valueHandle;

        log(`Sending notification (conn=${ch}, char=${charHandle}, "${valStr}")...`);
        const rc = await bridge.gattServerNotify(ch, charHandle, data);
        if (rc === 0) {
            log('Notification sent');
        } else {
            log(`Notify failed (${rc})`);
        }
    } catch (err) {
        log(`Notify error: ${err.message}`);
    }
}

async function btAdvertiseStart() {
    try {
        log('Starting BLE advertising...');
        const rc = await bridge.btAdvertiseStart();
        if (rc === 0) log('BLE advertising started');
        else log(`Advertise start failed (${rc})`);
    } catch (err) {
        log(`Advertise error: ${err.message}`);
    }
}

async function btAdvertiseStop() {
    try {
        log('Stopping BLE advertising...');
        const rc = await bridge.btAdvertiseStop();
        if (rc === 0) log('BLE advertising stopped');
        else log(`Advertise stop failed (${rc})`);
    } catch (err) {
        log(`Advertise error: ${err.message}`);
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
