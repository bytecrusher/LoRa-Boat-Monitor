function updateLoraPage(myObj) {
    setElementValue('quality', myObj.Device.NetworkParameter.ConnectionQuality.Value);
    setElementValue('region', myObj.Device.LoRaSettings.Frequency);
    setElementValue('address', myObj.Device.LoRaSettings.DeviceAddress);
    setElementValue('actualch', myObj.Device.LoRaSettings.ActualChannel);
    setElementValue('actualsf', myObj.Device.LoRaSettings.ActualSF);
    setElementValue('tinterval', myObj.Device.LoRaSettings.TXInterval);
    setElementValue('slot', myObj.Device.LoRaSettings.TimeSlot);
    setElementValue('counter', myObj.Device.LoRaSettings.TXCounter);
    setElementValue('info', myObj.Device.NetworkParameter.ServerMode == 4 ? '(Demo Mode)' : '');
}

function refreshLoraPage() {
    fetchJson('/data.json', updateLoraPage);
}

document.addEventListener('DOMContentLoaded', function () {
    refreshLoraPage();
    setInterval(refreshLoraPage, 1000);
});
