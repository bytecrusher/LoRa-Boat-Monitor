var staticData = null;
var BATTERY_MIN_VOLTAGE = 10.5;
var BATTERY_NORMAL_VOLTAGE = 12.4;
var BATTERY_MAX_VOLTAGE = 14.4;

function clampNumber(value, min, max) {
    var numberValue = parseFloat(value);
    if (isNaN(numberValue)) {
        numberValue = min;
    }

    return Math.max(min, Math.min(max, numberValue));
}

function resolveTankPercent(value, adcValue) {
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

function updateTankCard(prefix, value, adcValue) {
    var percent = resolveTankPercent(value, adcValue);
    var card = document.getElementById(prefix + 'Card');
    var instrument = document.getElementById(prefix + 'Instrument');
    var needle = document.getElementById(prefix + 'Needle');
    var gaugeValue = document.getElementById(prefix + 'GaugeValue');
    var status = document.getElementById(prefix + 'Status');
    var angle = -130 + (percent * 2.6);

    if (instrument) {
        instrument.style.setProperty('--tank-level', percent.toFixed(1));
    }

    if (needle) {
        needle.style.transform = 'translateX(-50%) rotate(' + angle.toFixed(1) + 'deg)';
    }

    if (gaugeValue) {
        gaugeValue.textContent = percent.toFixed(1) + '%';
    }

    if (!card || !status) {
        return;
    }

    card.classList.remove('tank-card--low', 'tank-card--empty', 'tank-card--ok');
    if (percent <= 5) {
        card.classList.add('tank-card--empty');
        status.textContent = 'Empty';
    } else if (percent < 20) {
        card.classList.add('tank-card--low');
        status.textContent = 'Low level';
    } else {
        card.classList.add('tank-card--ok');
        status.textContent = 'Level OK';
    }
}

function resolveBatteryGaugePercent(voltage, capacity) {
    var capacityPercent = parseFloat(capacity);
    if (!isNaN(capacityPercent) && capacityPercent >= 0) {
        return Math.max(0, Math.min(100, capacityPercent));
    }

    var clampedVoltage = clampNumber(voltage, BATTERY_MIN_VOLTAGE, BATTERY_MAX_VOLTAGE);
    return ((clampedVoltage - BATTERY_MIN_VOLTAGE) / (BATTERY_MAX_VOLTAGE - BATTERY_MIN_VOLTAGE)) * 100;
}

function updateBatteryGauge(voltage, capacity, rawAdc) {
    var numericVoltage = parseFloat(voltage);
    var percent = resolveBatteryGaugePercent(voltage, capacity);
    var card = document.getElementById('batteryCard');
    var needle = document.getElementById('batteryNeedle');
    var voltageValue = document.getElementById('batteryGaugeVoltage');
    var capacityValue = document.getElementById('batteryGaugeCapacity');
    var rawValue = document.getElementById('batteryGaugeAdc');
    var status = document.getElementById('batteryStatus');
    var angle = -130 + (percent * 2.6);

    if (needle) {
        needle.style.transform = 'translateX(-50%) rotate(' + angle.toFixed(1) + 'deg)';
    }

    if (voltageValue) {
        voltageValue.textContent = isNaN(numericVoltage) ? '-' : numericVoltage.toFixed(2);
    }

    if (capacityValue) {
        capacityValue.textContent = percent.toFixed(0) + '%';
    }

    if (rawValue) {
        rawValue.textContent = rawAdc || '0';
    }

    if (!card || !status) {
        return;
    }

    card.classList.remove('battery-card--low', 'battery-card--warning', 'battery-card--ok', 'battery-card--charge');
    if (!isNaN(numericVoltage) && numericVoltage > BATTERY_NORMAL_VOLTAGE + 0.4) {
        card.classList.add('battery-card--charge');
        status.textContent = 'Charging / high';
    } else if (percent <= 15) {
        card.classList.add('battery-card--low');
        status.textContent = 'Battery low';
    } else if (percent < 35) {
        card.classList.add('battery-card--warning');
        status.textContent = 'Watch battery';
    } else {
        card.classList.add('battery-card--ok');
        status.textContent = 'Battery OK';
    }
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
    updateBatteryGauge(
        myObj.Device.MeasuringValues.BatteryVoltage.Value,
        myObj.Device.MeasuringValues.BatteryCapacity.Value,
        myObj.Device.MeasuringValues.BatteryAdc.Value
    );
    updateTankCard('tank1', myObj.Device.MeasuringValues.Tank1.Value, myObj.Device.MeasuringValues.Tank1adc.Value);
    updateTankCard('tank2', myObj.Device.MeasuringValues.Tank2.Value, myObj.Device.MeasuringValues.Tank2adc.Value);
}

function refreshSensorPage() {
    fetchJson('/data.json', updateSensorPage);
}

document.addEventListener('DOMContentLoaded', function () {
    fetchJson('/staticdata.json', function (myObj) {
        staticData = myObj;
    });
    startVisiblePolling(refreshSensorPage, 5000);
});
