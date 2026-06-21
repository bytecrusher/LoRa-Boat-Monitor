var staticData = null;
var tank1Gauge = null;
var tank2Gauge = null;

function tankPercentToGaugeValue(value, adcValue) {
    var percent = parseFloat(value);
    if (isNaN(percent)) {
        percent = 0;
    }

    if (percent <= 0) {
        var rawAdc = parseFloat(adcValue);
        if (!isNaN(rawAdc) && rawAdc > 0) {
            percent = (rawAdc / 4095) * 100;
        }
    }

    percent = Math.max(0, Math.min(100, percent));
    return percent;
}

function updateGauge(gauge, value, adcValue) {
    if (!gauge) {
        return;
    }

    gauge.update({ value: tankPercentToGaugeValue(value, adcValue) });
}

function updateSensorIndicators(myObj) {
    toggleClass('alarm', 'led-red', myObj.Device.MeasuringValues.Alarm.Value != '0');
    toggleClass('relay', 'led-green', myObj.Device.MeasuringValues.Relay.Value != '0');
    toggleClass('standbyInputLed', 'led-red', myObj.Device.MeasuringValues.StandbyInputState.Value === 'Active');
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
    setElementValue('batteryAdc', myObj.Device.MeasuringValues.BatteryAdc.Value);
    setElementValue('batteryVoltageDiag', myObj.Device.MeasuringValues.BatteryVoltage.Value);
    setElementValue('capacity', myObj.Device.MeasuringValues.BatteryCapacity.Value);
    setElementValue('1wtemp', myObj.Device.MeasuringValues.Temp1Wire.Value);
    setElementValue('1wunit', myObj.Device.MeasuringValues.Temp1Wire.Unit);
    setElementValue('tank1', myObj.Device.MeasuringValues.Tank1.Value);
    setElementValue('tank1adc', myObj.Device.MeasuringValues.Tank1adc.Value);
    setElementValue('tank2', myObj.Device.MeasuringValues.Tank2.Value);
    setElementValue('tank2adc', myObj.Device.MeasuringValues.Tank2adc.Value);
    setElementValue('rtimer', myObj.Device.MeasuringValues.RelayTimer.Value);
    setElementValue('info', myObj.Device.NetworkParameter.ServerMode == 4 ? '(Demo Mode)' : '');
    setElementValue('standbyInputState', myObj.Device.MeasuringValues.StandbyInputState.Value);
    setElementValue('standbyInputPin', myObj.Device.MeasuringValues.StandbyInputPin.Value);
    setElementValue('standbyInputLevel', myObj.Device.MeasuringValues.StandbyInputLevel.Value);
    setElementValue('standbyModeDiag', myObj.Device.MeasuringValues.standbyMode.Value);
    setElementValue('standbyInputStateDiag', myObj.Device.MeasuringValues.StandbyInputState.Value);
    setElementValue('standbyInputPinDiag', myObj.Device.MeasuringValues.StandbyInputPin.Value);
    setElementValue('standbyInputLevelDiag', myObj.Device.MeasuringValues.StandbyInputLevel.Value);

    updateSensorIndicators(myObj);
    updateGauge(tank1Gauge, myObj.Device.MeasuringValues.Tank1.Value, myObj.Device.MeasuringValues.Tank1adc.Value);
    updateGauge(tank2Gauge, myObj.Device.MeasuringValues.Tank2.Value, myObj.Device.MeasuringValues.Tank2adc.Value);
}

function refreshSensorPage() {
    fetchJson('/data.json', updateSensorPage);
}

function createTankGauge(canvasId, title) {
    return new RadialGauge({
        renderTo: canvasId,
        width: 300,
        height: 300,
        units: '%',
        title: title,
        minValue: 0,
        startAngle: 90,
        ticksAngle: 180,
        valueBox: true,
        valueInt: 3,
        valueDec: 1,
        maxValue: 100,
        majorTicks: ['0', '50', '100'],
        minorTicks: 2,
        strokeTicks: true,
        highlights: [
            { from: 0, to: 20, color: 'rgba(200, 50, 50, .65)' },
            { from: 20, to: 100, color: 'rgba(0, 128, 0, .28)' }
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

function initGauges() {
    tank1Gauge = createTankGauge('canvas-id', 'Tank 1');
    tank2Gauge = createTankGauge('canvas-id2', 'Tank 2');
}

document.addEventListener('DOMContentLoaded', function () {
    initGauges();
    fetchJson('/staticdata.json', function (myObj) {
        staticData = myObj;
    });
    refreshSensorPage();
    setInterval(refreshSensorPage, 1000);
});
