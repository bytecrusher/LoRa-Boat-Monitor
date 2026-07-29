var indexLiveDataTimer = null;
var indexStaticDataTimer = null;

function setIndexNavigationDisabled(element, isDisabled) {
    if (element.tagName === 'A') {
        if (isDisabled) {
            if (!element.hasAttribute('data-diagnostic-href')) {
                element.setAttribute('data-diagnostic-href', element.getAttribute('href') || '');
                element.setAttribute('data-diagnostic-tabindex', element.getAttribute('tabindex') || '');
            }
            element.removeAttribute('href');
            element.setAttribute('tabindex', '-1');
        } else if (element.hasAttribute('data-diagnostic-href')) {
            var href = element.getAttribute('data-diagnostic-href');
            var tabindex = element.getAttribute('data-diagnostic-tabindex');
            if (href) {
                element.setAttribute('href', href);
            }
            if (tabindex) {
                element.setAttribute('tabindex', tabindex);
            } else {
                element.removeAttribute('tabindex');
            }
        }

        if (isDisabled) {
            element.setAttribute('aria-disabled', 'true');
        } else {
            element.removeAttribute('aria-disabled');
        }
    }

    element.disabled = isDisabled;
}

function updateIndexPage(myObj) {
    var serverMode = myObj.Device.NetworkParameter.ServerMode;
    var isDiagnosticMode = serverMode == 3;
    var network = myObj.Device.NetworkParameter || {};
    var settings = myObj.Device.DeviceSettings || {};
    var firmwareChannelLabel = myObj.Device.FirmwareChannelLabel ? ' (' + myObj.Device.FirmwareChannelLabel + ')' : '';

    setElementValue('info', isDiagnosticMode ? '(Diagnose Mode)' : '');
    ['sensorv', 'firmware', 'lora', 'restart', 'devinfo'].forEach(function (id) {
        var button = document.getElementById(id);
        if (button) {
            setIndexNavigationDisabled(button, isDiagnosticMode);
        }
    });

    setElementHidden('webserialRow', settings.WebSerialDebug == 0);
    setText('dashboardFirmware', (myObj.Device.FirmwareVersion || '-') + firmwareChannelLabel);
    setText('dashboardMdsUrl', network.MdsUrl || '-');
}

function updateIndexLiveData(myObj) {
    var values = myObj.Device.MeasuringValues || {};
    var mainPowerMode = values.MainPowerMode || values.StandbyInputState;
    var mainPowerPin = values.MainPowerInputPin || values.StandbyInputPin;
    var mainPowerLevel = values.MainPowerInputLevel || values.StandbyInputLevel;
    setText('dashboardBatteryVoltage', values.BatteryVoltage ? values.BatteryVoltage.Value : '-');
    setText('dashboardBatteryCapacity', values.BatteryCapacity ? values.BatteryCapacity.Value : '-');
    setText('dashboardStandbyState', mainPowerMode ? mainPowerMode.Value : '-');
    setText('dashboardStandbyPin', mainPowerPin ? mainPowerPin.Value : '-');
    setText('dashboardStandbyLevel', mainPowerLevel ? mainPowerLevel.Value : '-');
    setText('dashboardMdsState', values.SendDataViaWifi && values.SendDataViaWifi.Value === 'Yes' ? 'Enabled' : 'Disabled');
}

function setDashboardUpdateStatus(label, state) {
    setText('dashboardUpdateStatus', label);
    var dot = document.getElementById('dashboardUpdateDot');
    if (dot) {
        dot.className = 'update-indicator-dot update-indicator-' + (state || 'unknown');
    }
}

async function requestIndexJson(url) {
    var response = await fetch(url, { cache: 'no-store', credentials: 'same-origin' });
    var payload = await response.json();
    if (!response.ok) {
        throw new Error(payload && payload.message ? payload.message : 'Request failed');
    }
    return payload;
}

async function refreshDashboardUpdateStatus() {
    setDashboardUpdateStatus('Checking for updates...', 'checking');
    try {
        var webInfo = await requestIndexJson('/updatefilesinfo?ts=' + Date.now());
        var firmwareInfo = null;
        for (var attempt = 0; attempt < 20; attempt += 1) {
            firmwareInfo = await requestIndexJson('/mdsotainfo?' + (attempt === 0 ? 'refresh=1&' : '') + 'ts=' + Date.now());
            if (!firmwareInfo.running) {
                break;
            }
            await new Promise(function (resolve) { setTimeout(resolve, 500); });
        }

        var firmwareAvailable = firmwareInfo && firmwareInfo.status === 'update-available';
        var webAvailable = webInfo && !webInfo.upToDate;
        if (firmwareAvailable || webAvailable) {
            setDashboardUpdateStatus('Update available', 'available');
        } else if (firmwareInfo && (firmwareInfo.status === 'current' || firmwareInfo.status === 'device-newer') && webInfo.upToDate) {
            setDashboardUpdateStatus('System up to date', 'current');
        } else {
            setDashboardUpdateStatus('Check update status', 'unknown');
        }
    } catch (error) {
        setDashboardUpdateStatus('Update check unavailable', 'unknown');
    }
}

document.addEventListener('DOMContentLoaded', function () {
    var webserialButton = document.getElementById('webserialRow');
    if (webserialButton) {
        webserialButton.addEventListener('click', function () {
            navigateTo('/webserial', '_blank');
        });
    }

    function refreshStaticData() {
        fetchJson('/staticdata.json?ts=' + Date.now(), updateIndexPage);
    }

    function refreshLiveData() {
        fetchJson('/data.json?ts=' + Date.now(), updateIndexLiveData);
    }

    refreshStaticData();
    refreshDashboardUpdateStatus();

    indexLiveDataTimer = startVisiblePolling(refreshLiveData, 5000);
    indexStaticDataTimer = startVisiblePolling(refreshStaticData, 15000);
});

window.addEventListener('beforeunload', function () {
    if (indexLiveDataTimer) {
        indexLiveDataTimer();
        indexLiveDataTimer = null;
    }
    if (indexStaticDataTimer) {
        indexStaticDataTimer();
        indexStaticDataTimer = null;
    }
});
