var indexLiveDataTimer = null;
var indexStaticDataTimer = null;
var indexWebFilesTimer = null;

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
    setText('dashboardBatteryVoltage', values.BatteryVoltage ? values.BatteryVoltage.Value : '-');
    setText('dashboardBatteryCapacity', values.BatteryCapacity ? values.BatteryCapacity.Value : '-');
    setText('dashboardStandbyState', values.StandbyInputState ? values.StandbyInputState.Value : '-');
    setText('dashboardStandbyPin', values.StandbyInputPin ? values.StandbyInputPin.Value : '-');
    setText('dashboardStandbyLevel', values.StandbyInputLevel ? values.StandbyInputLevel.Value : '-');
    setText('dashboardMdsState', values.SendDataViaWifi && values.SendDataViaWifi.Value === 'Yes' ? 'Enabled' : 'Disabled');
}

function updateWebFilesInfo(info) {
    var status = info.upToDate ? 'Up to date' : 'Update available';
    var storedChannel = info.storedWebFilesChannel ? ' (' + info.storedWebFilesChannel + ')' : '';
    var firmwareChannel = info.firmwareChannelLabel ? ' (' + info.firmwareChannelLabel + ')' : '';
    setText('dashboardWebFiles', status);
    setText('dashboardWebFilesDetail', 'Installed: ' + (info.storedWebFilesVersion || '-') + storedChannel + ', firmware: ' + (info.firmwareVersion || '-') + firmwareChannel);
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

    function refreshWebFilesStatus() {
        fetchJson('/updatefilesinfo?ts=' + Date.now(), updateWebFilesInfo, function () {
            setText('dashboardWebFiles', 'Unknown');
            setText('dashboardWebFilesDetail', 'Status endpoint not available');
        });
    }

    refreshStaticData();
    refreshWebFilesStatus();

    indexLiveDataTimer = startVisiblePolling(refreshLiveData, 5000);
    indexStaticDataTimer = setInterval(refreshStaticData, 15000);
    indexWebFilesTimer = setInterval(refreshWebFilesStatus, 10000);
});

window.addEventListener('beforeunload', function () {
    if (indexLiveDataTimer) {
        indexLiveDataTimer();
        indexLiveDataTimer = null;
    }
    if (indexStaticDataTimer) {
        clearInterval(indexStaticDataTimer);
        indexStaticDataTimer = null;
    }
    if (indexWebFilesTimer) {
        clearInterval(indexWebFilesTimer);
        indexWebFilesTimer = null;
    }
});
