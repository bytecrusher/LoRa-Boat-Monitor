function updateLoraPage(myObj) {
    var device = myObj && myObj.Device ? myObj.Device : {};
    var network = device.NetworkParameter || {};
    var settings = device.LoRaSettings || {};
    setElementValue('quality', network.ConnectionQuality && network.ConnectionQuality.Value);
    setElementValue('region', settings.Frequency || '-');
    setElementValue('address', settings.DeviceAddress || '-');
    setElementValue('actualch', settings.ActualChannel || '-');
    setElementValue('actualsf', settings.ActualSF || '-');
    setElementValue('tinterval', settings.TXInterval || '-');
    setElementValue('slot', settings.TimeSlot || '-');
    setElementValue('counter', settings.TXCounter || '-');
    setElementValue('info', network.ServerMode == 4 ? '(Demo Mode)' : '');
}

function refreshLoraPage() {
    fetchJson('/data.json?ts=' + Date.now(), updateLoraPage);
}

function formatLoraUptime(milliseconds) {
    var totalSeconds = Math.max(0, Math.floor((Number(milliseconds) || 0) / 1000));
    var hours = Math.floor(totalSeconds / 3600);
    var minutes = Math.floor((totalSeconds % 3600) / 60);
    var seconds = totalSeconds % 60;
    return (hours ? hours + 'h ' : '') + minutes + 'm ' + seconds + 's';
}

function manualLoraStateLabel(state) {
    var labels = {
        idle: 'Idle',
        queued: 'Queued',
        preparing: 'Preparing',
        transmitting: 'Transmitting',
        complete: 'Complete',
        error: 'Error'
    };
    return labels[state] || state || 'Unknown';
}

function renderManualLoraEvents(events) {
    var log = document.getElementById('loraDebugLog');
    if (!log) return;
    log.replaceChildren();

    if (!Array.isArray(events) || events.length === 0) {
        var emptyItem = document.createElement('li');
        emptyItem.textContent = 'No manual transmission events recorded.';
        log.appendChild(emptyItem);
        return;
    }

    events.forEach(function (event) {
        var item = document.createElement('li');
        var time = document.createElement('time');
        time.textContent = '+' + formatLoraUptime(event.millis);
        var message = document.createElement('span');
        message.textContent = event.message || '-';
        item.appendChild(time);
        item.appendChild(message);
        log.appendChild(item);
    });
}

function updateManualLoraStatus(status) {
    var state = status.state || 'idle';
    var button = document.getElementById('sendLoraNow');
    var notice = document.getElementById('loraActionNotice');

    setElementValue('manualLoraState', manualLoraStateLabel(state));
    setElementValue('manualLoraPending', status.txPending ? 'TX/RX busy' : 'Radio ready');
    setElementValue('loraUptime', formatLoraUptime(status.uptimeMillis));

    var result = 'No completed test';
    if (status.completedMillis) {
        result = status.acknowledged ? 'ACK received' : 'Radio cycle complete';
        if (status.downlinkBytes > 0) {
            result += ', ' + status.downlinkBytes + ' B downlink';
        }
    }
    setElementValue('manualLoraResult', result);

    if (button) {
        button.disabled = Boolean(status.busy);
        button.textContent = status.busy ? 'LoRa Send in Progress...' : 'Send LoRa Packet Now';
    }

    if (notice) {
        notice.textContent = status.message || 'Ready for a manual LoRa uplink.';
        notice.className = 'notice lora-inline-notice ' +
            (state === 'error' ? 'notice-error' : state === 'complete' ? 'notice-success' : 'notice-info');
    }
    renderManualLoraEvents(status.events);
}

function showManualLoraStatusError(error) {
    var notice = document.getElementById('loraActionNotice');
    if (!notice) return;
    notice.className = 'notice notice-error lora-inline-notice';
    notice.textContent = error && error.message
        ? error.message
        : error && error.json && error.json.message
            ? error.json.message
            : 'LoRa status request failed.';
}

function refreshManualLoraStatus() {
    fetchJson('/lora/status?ts=' + Date.now(), updateManualLoraStatus, showManualLoraStatusError);
}

async function sendManualLoraPacket() {
    var button = document.getElementById('sendLoraNow');
    var notice = document.getElementById('loraActionNotice');
    if (button) button.disabled = true;
    if (notice) {
        notice.className = 'notice notice-info lora-inline-notice';
        notice.textContent = 'Requesting manual LoRa uplink...';
    }

    try {
        var response = await request('/lora/send', { method: 'POST' });
        var message = response.json && response.json.message ? response.json.message : response.text;
        if (!response.ok) {
            throw { message: message || 'Manual LoRa uplink request failed.' };
        }
        if (notice) notice.textContent = message || 'Manual LoRa uplink queued.';
        refreshManualLoraStatus();
    } catch (error) {
        showManualLoraStatusError(error);
        if (button) button.disabled = false;
    }
}

document.addEventListener('DOMContentLoaded', function () {
    var sendButton = document.getElementById('sendLoraNow');
    if (sendButton) sendButton.addEventListener('click', sendManualLoraPacket);
    startVisiblePolling(refreshLoraPage, 5000);
    startVisiblePolling(refreshManualLoraStatus, 1500);
});
