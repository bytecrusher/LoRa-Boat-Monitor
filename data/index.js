function updateIndexPage(myObj) {
    var serverMode = myObj.Device.NetworkParameter.ServerMode;
    var isDiagnosticMode = serverMode == 3;
    var network = myObj.Device.NetworkParameter || {};
    var settings = myObj.Device.DeviceSettings || {};

    setElementValue('info', isDiagnosticMode ? '(Diagnose Mode)' : '');
    ['sensorv', 'firmware', 'lora', 'restart', 'devinfo'].forEach(function (id) {
        var button = document.getElementById(id);
        if (button) {
            button.disabled = isDiagnosticMode;
        }
    });

    setElementHidden('webserialRow', settings.WebSerialDebug == 0);
    setText('dashboardFirmware', myObj.Device.FirmwareVersion || '-');
    setText('dashboardMdsState', network.MdsUrl ? 'Configured' : 'Missing URL');
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
    setText('dashboardWebFiles', status);
    setText('dashboardWebFilesDetail', 'Installed: ' + (info.storedWebFilesVersion || '-') + ', firmware: ' + (info.firmwareVersion || '-'));
}

document.addEventListener('DOMContentLoaded', function () {
    var webserialButton = document.getElementById('webserialRow');
    if (webserialButton) {
        webserialButton.addEventListener('click', function () {
            navigateTo('/webserial', '_blank');
        });
    }

    fetchJson('/staticdata.json', updateIndexPage);
    fetchJson('/data.json', updateIndexLiveData);
    fetchJson('/updatefilesinfo', updateWebFilesInfo, function () {
        setText('dashboardWebFiles', 'Unknown');
        setText('dashboardWebFilesDetail', 'Status endpoint not available');
    });
});
