var xmlhttp = new XMLHttpRequest();
xmlhttp.onreadystatechange = function () {
  if (this.readyState == 4 && this.status == 200) {
    var myObj = JSON.parse(this.responseText);

    //cssid = document.getElementById('cssid');
    //cssid.value = myObj.Device.NetworkParameter.WLANClientSSID;
    //sssid = document.getElementById('sssid');
    //sssid.value = myObj.Device.NetworkParameter.WLANServerSSID;

    heapsize = document.getElementById('heapsize');
    heapsize.value = myObj.Device.ESP32.FreeHeapSize.Value;
    document.getElementById('hunit').innerHTML = myObj.Device.ESP32.FreeHeapSize.Unit;

    strength = document.getElementById('strength');
    strength.value = myObj.Device.NetworkParameter.FieldStrength.Value;
    document.getElementById('sunit').innerHTML = myObj.Device.NetworkParameter.FieldStrength.Unit;

    quality = document.getElementById('quality');
    quality.value = myObj.Device.NetworkParameter.ConnectionQuality.Value;
    if (document.getElementById('qunit') != null) {
      document.getElementById('qunit').innerHTML = myObj.Device.NetworkParameter.ConnectionQuality.Unit;
    }
    document.getElementById('quality').innerHTML = myObj.Device.NetworkParameter.ConnectionQuality.Value;
    if (document.getElementById('qunit2') != null) {
      document.getElementById('qunit2').innerHTML = myObj.Device.NetworkParameter.ConnectionQuality.Unit;
      document.getElementById('quality2').value = myObj.Device.NetworkParameter.ConnectionQuality.Value;
    }

    actualch = document.getElementById('actualch');
    actualch.value = myObj.Device.LoRaSettings.ActualChannel;
    actualsf = document.getElementById('actualsf');
    actualsf.value = myObj.Device.LoRaSettings.ActualSF;
    tinterval = document.getElementById('tinterval');
    tinterval.value = myObj.Device.LoRaSettings.TXInterval * 30;
    slot = document.getElementById('slot');
    slot.value = myObj.Device.LoRaSettings.TimeSlot;
    counter = document.getElementById('counter');
    counter.value = myObj.Device.LoRaSettings.TXCounter;

    lat = document.getElementById('lat');
    lat.value = myObj.Device.MeasuringValues.Latitude.Value;

    lon = document.getElementById('lon');
    lon.value = myObj.Device.MeasuringValues.Longitude.Value;

    alt = document.getElementById('alt');
    alt.value = myObj.Device.MeasuringValues.Altitude.Value;

    //if (String(actconf.envSensor) == "BME280") { // from cpp
    if (envSensor.value == "BME280") {
      airtemp = document.getElementById('airtemp');
      airtemp.value = myObj.Device.MeasuringValues.AirTemperature.Value;
      document.getElementById('atunit').innerHTML = myObj.Device.MeasuringValues.AirTemperature.Unit;

      pressure = document.getElementById('pressure');
      pressure.value = myObj.Device.MeasuringValues.AirPressure.Value;

      humidity = document.getElementById('humidity');
      humidity.value = myObj.Device.MeasuringValues.AirHumidity.Value;

      dewpoint = document.getElementById('dewpoint');
      dewpoint.value = myObj.Device.MeasuringValues.Dewpoint.Value;
      document.getElementById('dpunit').innerHTML = myObj.Device.MeasuringValues.Dewpoint.Unit;
    }  // from cpp

    voltage = document.getElementById('voltage');
    voltage.value = myObj.Device.MeasuringValues.BatteryVoltage.Value;

    temp1w = document.getElementById('1wtemp');
    temp1w.value = myObj.Device.MeasuringValues.Temp1Wire.Value;
    document.getElementById('1wunit').innerHTML = myObj.Device.MeasuringValues.Temp1Wire.Unit;

    tank1 = document.getElementById('tank1');
    tank1.value = myObj.Device.MeasuringValues.Tank1Voltage.Value;

    tank1adc = document.getElementById('tank1adc');
    tank1adc.value = myObj.Device.MeasuringValues.Tank1adc.Value;

    tank2 = document.getElementById('tank2');
    tank2.value = myObj.Device.MeasuringValues.Tank2Voltage.Value;

    tank2adc = document.getElementById('tank2adc');
    tank2adc.value = myObj.Device.MeasuringValues.Tank2adc.Value;

    alarm = document.getElementById('alarm');
    alarm.value = myObj.Device.MeasuringValues.Alarm.Value;

    relay = document.getElementById('relay');
    relay.value = myObj.Device.MeasuringValues.Relay.Value;

    rtimer = document.getElementById('rtimer');
    rtimer.value = myObj.Device.MeasuringValues.RelayTimer.Value;

    envSensor = document.getElementById('envSensor');
    envSensor.value = myObj.Device.MeasuringValues.EnvSensor.Value;

    standbyMode = document.getElementById('standbyMode');
    standbyMode.value = myObj.Device.MeasuringValues.standbyMode.Value;

    loraOperationMode = document.getElementById('loraOperationMode');
    loraOperationMode.value = myObj.Device.MeasuringValues.loraOperationMode.Value;

    WifiStandbyMode = document.getElementById('WifiStandbyMode');
    WifiStandbyMode.value = myObj.Device.MeasuringValues.WifiStandbyMode.Value;

    // If Demo Mode active the give out a message
    servermode = myObj.Device.NetworkParameter.ServerMode;
    if (servermode == 4) {
      document.getElementById('info').innerHTML = '(Demo Mode)';
    } else {
      document.getElementById('info').innerHTML = '';
    }
  }
};


var xmlhttpStaticData = new XMLHttpRequest();
xmlhttpStaticData.onreadystatechange = function () {
  if (this.readyState == 4 && this.status == 200) {
    var myObj = JSON.parse(this.responseText);

    cssid1 = document.getElementById('cssid1');
    cssid1.value = myObj.Device.NetworkParameter.WLANClientSSID;
    sssid = document.getElementById('sssid');
    sssid.value = myObj.Device.NetworkParameter.WLANServerSSID;

    actualch = document.getElementById('actualch');
    actualch.value = myObj.Device.LoRaSettings.ActualChannel;
    actualsf = document.getElementById('actualsf');
    actualsf.value = myObj.Device.LoRaSettings.ActualSF;
    tinterval = document.getElementById('tinterval');
    tinterval.value = myObj.Device.LoRaSettings.TXInterval * 30;
    slot = document.getElementById('slot');
    slot.value = myObj.Device.LoRaSettings.TimeSlot;
    counter = document.getElementById('counter');
    counter.value = myObj.Device.LoRaSettings.TXCounter;

    standbyMode = document.getElementById('standbyMode');
    standbyMode.value = myObj.Device.MeasuringValues.standbyMode.Value;

    loraOperationMode = document.getElementById('loraOperationMode');
    loraOperationMode.value = myObj.Device.MeasuringValues.loraOperationMode.Value;

    // If Demo Mode active the give out a message
    servermode = myObj.Device.NetworkParameter.ServerMode;
    if (servermode == 4) {
      document.getElementById('info').innerHTML = '(Demo Mode)';
    } else {
      document.getElementById('info').innerHTML = '';
    }
  }
};

function read_json() {
  xmlhttp.open('GET', '/data.json', true);
  xmlhttp.send();
}

setInterval(function () { read_json(); }, 1000);
