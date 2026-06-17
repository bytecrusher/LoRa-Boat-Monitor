function setNetworkInfo(myObj) {
  setElementValue('heapsize', myObj.Device.ESP32.FreeHeapSize.Value);
  setElementValue('hunit', myObj.Device.ESP32.FreeHeapSize.Unit);
  setElementValue('strength', myObj.Device.NetworkParameter.FieldStrength.Value);
  setElementValue('sunit', myObj.Device.NetworkParameter.FieldStrength.Unit);
  setElementValue('quality', myObj.Device.NetworkParameter.ConnectionQuality.Value);
  setElementValue('quality2', myObj.Device.NetworkParameter.ConnectionQuality.Value);
  setElementValue('qunit', myObj.Device.NetworkParameter.ConnectionQuality.Unit);
  setElementValue('qunit2', myObj.Device.NetworkParameter.ConnectionQuality.Unit);
}

function setLoRaInfo(myObj) {
  setElementValue('actualch', myObj.Device.LoRaSettings.ActualChannel);
  setElementValue('actualsf', myObj.Device.LoRaSettings.ActualSF);
  setElementValue('tinterval', myObj.Device.LoRaSettings.TXInterval);
  setElementValue('slot', myObj.Device.LoRaSettings.TimeSlot);
  setElementValue('counter', myObj.Device.LoRaSettings.TXCounter);
}

function setPositionInfo(myObj) {
  setElementValue('lat', myObj.Device.MeasuringValues.Latitude.Value);
  setElementValue('lon', myObj.Device.MeasuringValues.Longitude.Value);
  setElementValue('alt', myObj.Device.MeasuringValues.Altitude.Value);
}

function setEnvironmentInfo(myObj) {
  if (myObj.Device.MeasuringValues.EnvSensor.Value !== 'BME280') {
    return;
  }

  setElementValue('airtemp', myObj.Device.MeasuringValues.AirTemperature.Value);
  setElementValue('atunit', myObj.Device.MeasuringValues.AirTemperature.Unit);
  setElementValue('pressure', myObj.Device.MeasuringValues.AirPressure.Value);
  setElementValue('humidity', myObj.Device.MeasuringValues.AirHumidity.Value);
  setElementValue('dewpoint', myObj.Device.MeasuringValues.Dewpoint.Value);
  setElementValue('dpunit', myObj.Device.MeasuringValues.Dewpoint.Unit);
  setElementValue('dunit', myObj.Device.MeasuringValues.Dewpoint.Unit);
}

function setSensorInfo(myObj) {
  setElementValue('voltage', myObj.Device.MeasuringValues.BatteryVoltage.Value);
  setElementValue('1wtemp', myObj.Device.MeasuringValues.Temp1Wire.Value);
  setElementValue('1wunit', myObj.Device.MeasuringValues.Temp1Wire.Unit);
  setElementValue('tank1', myObj.Device.MeasuringValues.Tank1Voltage.Value);
  setElementValue('tank1adc', myObj.Device.MeasuringValues.Tank1adc.Value);
  setElementValue('tank2', myObj.Device.MeasuringValues.Tank2Voltage.Value);
  setElementValue('tank2adc', myObj.Device.MeasuringValues.Tank2adc.Value);
  setElementValue('alarm', myObj.Device.MeasuringValues.Alarm.Value);
  setElementValue('standbyInputState', myObj.Device.MeasuringValues.StandbyInputState.Value);
  setElementValue('standbyInputLevel', myObj.Device.MeasuringValues.StandbyInputLevel.Value);
  setElementValue('standbyInputPin', myObj.Device.MeasuringValues.StandbyInputPin.Value);
  setElementValue('relay', myObj.Device.MeasuringValues.Relay.Value);
  setElementValue('rtimer', myObj.Device.MeasuringValues.RelayTimer.Value);
  setElementValue('envSensor', myObj.Device.MeasuringValues.EnvSensor.Value);
  setElementValue('standbyMode', myObj.Device.MeasuringValues.standbyMode.Value);
  setElementValue('loraOperationMode', myObj.Device.MeasuringValues.loraOperationMode.Value);
  setElementValue('WifiStandbyMode', myObj.Device.MeasuringValues.WifiStandbyMode.Value);
}

function setServerModeInfo(myObj) {
  var infoElement = document.getElementById('info');
  if (!infoElement) {
    return;
  }

  if (myObj.Device.NetworkParameter.ServerMode == 4) {
    infoElement.textContent = '(Demo Mode)';
  } else {
    infoElement.textContent = '';
  }
}

function read_json() {
  fetchJson('/data.json', function (myObj) {
    setNetworkInfo(myObj);
    setLoRaInfo(myObj);
    setPositionInfo(myObj);
    setEnvironmentInfo(myObj);
    setSensorInfo(myObj);
    setServerModeInfo(myObj);
  });
}

function read_static_json() {
  fetchJson('/staticdata.json', function (myObj) {
    setElementValue('cssid1', myObj.Device.NetworkParameter.WLANClientSSID1);
    setElementValue('cssid2', myObj.Device.NetworkParameter.WLANClientSSID2);
    setElementValue('cssid3', myObj.Device.NetworkParameter.WLANClientSSID3);
    setElementValue('sssid', myObj.Device.NetworkParameter.WLANServerSSID);
    setElementValue('standbyMode', myObj.Device.DeviceSettings.standbyMode);

    var infoElement = document.getElementById('info');
    if (infoElement && myObj.Device.NetworkParameter.ServerMode == 4) {
      infoElement.textContent = '(Demo Mode)';
    }
  });
}

document.addEventListener('DOMContentLoaded', function () {
  read_json();
  read_static_json();
});

setInterval(function () { read_json(); }, 1000);
