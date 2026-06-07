function updateIndexPage(myObj) {
    var serverMode = myObj.Device.NetworkParameter.ServerMode;
    var isDiagnosticMode = serverMode == 3;

    setElementValue('info', isDiagnosticMode ? '(Diagnose Mode)' : '');
    ['sensorv', 'firmware', 'lora', 'restart', 'devinfo'].forEach(function (id) {
        var button = document.getElementById(id);
        if (button) {
            button.disabled = isDiagnosticMode;
        }
    });

    setElementHidden('webserialRow', myObj.Device.DeviceSettings.WebSerialDebug == 0);
}

document.addEventListener('DOMContentLoaded', function () {
    var webserialButton = document.getElementById('webserialRow');
    if (webserialButton) {
        webserialButton.addEventListener('click', function () {
            navigateTo('/webserial', '_blank');
        });
    }

    fetchJson('/staticdata.json', updateIndexPage);
});
