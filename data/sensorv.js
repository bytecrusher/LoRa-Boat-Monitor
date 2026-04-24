var staticData = null;
var gauge = null;

function updateSensorIndicators(myObj) {
    toggleClass('alarm', 'led-red', myObj.Device.MeasuringValues.Alarm.Value != '0');
    toggleClass('relay', 'led-green', myObj.Device.MeasuringValues.Relay.Value != '0');
}

function updateEnvironmentValues(myObj) {
    if (!staticData || !staticData.Device || staticData.Device.DeviceSettings.envSensor != 'BME280') {
        return;
    }

    setElementValue('airtemp', myObj.Device.MeasuringValues.AirTemperature.Value);
    setElementValue('atunit', myObj.Device.MeasuringValues.AirTemperature.Unit);
    setElementValue('pressure', myObj.Device.MeasuringValues.AirPressure.Value);
    setElementValue('humidity', myObj.Device.MeasuringValues.AirHumidity.Value);
    setElementValue('dewpoint', myObj.Device.MeasuringValues.Dewpoint.Value);
    setElementValue('dunit', myObj.Device.MeasuringValues.Dewpoint.Unit);
}

function updateSensorPage(myObj) {
    setElementValue('quality', myObj.Device.NetworkParameter.ConnectionQuality.Value);
    updateEnvironmentValues(myObj);

    setElementValue('lat', myObj.Device.MeasuringValues.Latitude.Value);
    setElementValue('latns', myObj.Device.MeasuringValues.Latitude.Unit);
    setElementValue('lon', myObj.Device.MeasuringValues.Longitude.Value);
    setElementValue('lonew', myObj.Device.MeasuringValues.Longitude.Unit);
    setElementValue('alt', myObj.Device.MeasuringValues.Altitude.Value);
    setElementValue('date', myObj.Device.MeasuringValues.Date.Value);
    setElementValue('time', myObj.Device.MeasuringValues.Time.Value);
    setElementValue('sunrise', myObj.Device.MeasuringValues.Sunrise.Value);
    setElementValue('sunset', myObj.Device.MeasuringValues.Sunset.Value);
    setElementValue('speed', myObj.Device.MeasuringValues.Speed.Value);
    setElementValue('course', myObj.Device.MeasuringValues.Course.Value);
    setElementValue('voltage', myObj.Device.MeasuringValues.BatteryVoltage.Value);
    setElementValue('capacity', myObj.Device.MeasuringValues.BatteryCapacity.Value);
    setElementValue('1wtemp', myObj.Device.MeasuringValues.Temp1Wire.Value);
    setElementValue('1wunit', myObj.Device.MeasuringValues.Temp1Wire.Unit);
    setElementValue('tank2', myObj.Device.MeasuringValues.Tank2.Value);
    setElementValue('tank2adc', myObj.Device.MeasuringValues.Tank2adc.Value);
    setElementValue('rtimer', myObj.Device.MeasuringValues.RelayTimer.Value);
    setElementValue('info', myObj.Device.NetworkParameter.ServerMode == 4 ? '(Demo Mode)' : '');

    updateSensorIndicators(myObj);
}

function refreshSensorPage() {
    fetchJson('/data.json', updateSensorPage);
}

function refreshTank1Gauge() {
    fetchJson('/getdata?data=Tank1', function (myObj2) {
        setElementValue('tank1', myObj2.Tank1);
        setElementValue('tank1adc', myObj2.Tank1adc);

        if (gauge) {
            gauge.value = ((myObj2.Tank1 / (100 / 80)) - 40);
            gauge.update();
        }
    });
}

function initGauge() {
    gauge = new RadialGauge({
        renderTo: 'canvas-id',
        width: 300,
        height: 300,
        units: '',
        title: 'Tank Level',
        minValue: -40,
        startAngle: 300,
        ticksAngle: 120,
        valueBox: false,
        maxValue: 40,
        majorTicks: ['-40', '0', '40'],
        minorTicks: 2,
        strokeTicks: true,
        highlights: [
            { from: 0, to: 40, color: 'rgba(200, 50, 50, .75)' },
            { from: -40, to: 0, color: 'rgba(0, 128, 0, .3)' }
        ],
        colorPlate: '#fff',
        borderShadowWidth: 0,
        borders: false,
        needleType: 'arrow',
        needleWidth: 4,
        needleCircleSize: 7,
        needleCircleOuter: true,
        needleCircleInner: false,
        animationDuration: 500,
        animationRule: 'linear'
    }).draw();
}

document.addEventListener('DOMContentLoaded', function () {
    initGauge();
    fetchJson('/staticdata.json', function (myObj) {
        staticData = myObj;
    });
    refreshSensorPage();
    refreshTank1Gauge();
    setInterval(refreshSensorPage, 1000);
    setInterval(refreshTank1Gauge, 250);
});
