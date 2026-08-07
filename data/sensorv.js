var staticData = null;
var BATTERY_MIN_VOLTAGE = 10.5;
var BATTERY_NORMAL_VOLTAGE = 12.4;
var BATTERY_MAX_VOLTAGE = 14.4;

function isUsableTemperature(value) {
    var numericValue = parseFloat(value);
    return !isNaN(numericValue) && numericValue > -90;
}

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

function resolveTemperatureReading(myObj) {
    var measuringValues = myObj.Device.MeasuringValues;
    var names = myObj.Device.SensorNames || {};
    var deviceSettings = staticData && staticData.Device ? (staticData.Device.DeviceSettings || {}) : {};
    var hasTemperatureConfig = Object.prototype.hasOwnProperty.call(deviceSettings, 'TempSensorType');
    var tempSensorType = String(deviceSettings.TempSensorType || '').toLowerCase();
    var envSensor = String(deviceSettings.envSensor || '').toLowerCase();
    var oneWire = measuringValues.Temp1Wire || {};
    var air = measuringValues.AirTemperature || {};

    if ((tempSensorType === 'ds18b20' || envSensor === 'vedirect-read') && isUsableTemperature(oneWire.Value)) {
        return {
            value: parseFloat(oneWire.Value),
            unit: oneWire.Unit || deviceSettings.TempUnit || 'C',
            source: envSensor === 'vedirect-read' ? (names.Vedirect || 'VE.Direct') : (names.Temperature || 'DS18B20')
        };
    }

    if (envSensor === 'bme280' && isUsableTemperature(air.Value)) {
        return {
            value: parseFloat(air.Value),
            unit: air.Unit || deviceSettings.TempUnit || 'C',
            source: names.Environment || 'BME280'
        };
    }

    if (hasTemperatureConfig) {
        return null;
    }

    // Keep the gauge compatible with older firmware that did not expose the sensor type.
    if (isUsableTemperature(oneWire.Value)) {
        return { value: parseFloat(oneWire.Value), unit: oneWire.Unit || 'C', source: 'External sensor' };
    }

    return null;
}

function updateTemperatureGauge(myObj) {
    var reading = resolveTemperatureReading(myObj);
    var card = document.getElementById('temperatureCard');
    var needle = document.getElementById('temperatureNeedle');
    var value = document.getElementById('temperatureGaugeValue');
    var readout = document.getElementById('temperatureGaugeReadout');
    var unit = document.getElementById('temperatureGaugeUnit');
    var source = document.getElementById('temperatureSource');
    var status = document.getElementById('temperatureStatus');
    var scale = { min: -20, mid: 30, max: 80, cold: 0, warm: 45, hot: 65 };

    if (!card || !status) {
        return;
    }

    card.classList.remove('temperature-card--unavailable', 'temperature-card--cold', 'temperature-card--normal', 'temperature-card--warm', 'temperature-card--hot');

    if (!reading) {
        card.classList.add('temperature-card--unavailable');
        status.textContent = 'Not available';
        if (value) {
            value.textContent = '-';
        }
        if (readout) {
            readout.textContent = '-';
        }
        if (unit) {
            unit.textContent = '';
        }
        if (source) {
            source.textContent = 'No active sensor';
        }
        if (needle) {
            needle.style.transform = 'translateX(-50%) rotate(-130deg)';
        }
        return;
    }

    var normalizedUnit = String(reading.unit).toUpperCase().replace('\u00b0', '');
    if (normalizedUnit === 'F') {
        scale = { min: -4, mid: 86, max: 176, cold: 32, warm: 113, hot: 149 };
    }

    var percent = ((clampNumber(reading.value, scale.min, scale.max) - scale.min) / (scale.max - scale.min)) * 100;
    var angle = -130 + (percent * 2.6);
    var displayUnit = '\u00b0' + normalizedUnit;

    if (needle) {
        needle.style.transform = 'translateX(-50%) rotate(' + angle.toFixed(1) + 'deg)';
    }
    if (value) {
        value.textContent = reading.value.toFixed(1);
    }
    if (readout) {
        readout.textContent = reading.value.toFixed(1) + displayUnit;
    }
    if (unit) {
        unit.textContent = displayUnit;
    }
    if (source) {
        source.textContent = reading.source;
    }
    setElementValue('temperatureGaugeMin', scale.min);
    setElementValue('temperatureGaugeMid', scale.mid);
    setElementValue('temperatureGaugeMax', scale.max);

    if (reading.value < scale.cold) {
        card.classList.add('temperature-card--cold');
        status.textContent = 'Cold';
    } else if (reading.value >= scale.hot) {
        card.classList.add('temperature-card--hot');
        status.textContent = 'Hot';
    } else if (reading.value >= scale.warm) {
        card.classList.add('temperature-card--warm');
        status.textContent = 'Warm';
    } else {
        card.classList.add('temperature-card--normal');
        status.textContent = 'Temperature OK';
    }
}

function updateSensorIndicators(myObj) {
    var mainPowerOn = myObj.Device.MeasuringValues.MainPowerOn || myObj.Device.MeasuringValues.Alarm;
    var mainPowerMode = myObj.Device.MeasuringValues.MainPowerMode || myObj.Device.MeasuringValues.StandbyInputState;
    toggleClass('alarm', 'led-green', mainPowerOn.Value != '0');
    toggleClass('relay', 'led-green', myObj.Device.MeasuringValues.Relay.Value != '0');
    toggleClass('standbyInputLed', 'led-green', mainPowerMode.Value === 'Always on');
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

function updateSensorNames(myObj) {
    var names = myObj.Device.SensorNames || {};
    var tankName = names.Tanks || 'Tanks';

    setText('batterySectionName', names.Battery || 'Battery');
    setText('batteryGaugeName', names.Battery || 'Battery');
    setText('batteryDiagnosticName', names.Battery || 'Battery');
    setText('temperatureTableName', names.Temperature || 'Temperature');
    setText('temperatureGaugeName', names.Temperature || 'Temperature');
    setText('tank1SensorName', tankName + ' 1');
    setText('tank2SensorName', tankName + ' 2');
    setText('statusSensorName', names.Status || 'Status');
    setText('gpsSensorName', names.Gps || 'GPS');
    setText('environmentSensorName', names.Environment || 'Environment');
    setText('dewpointSensorName', names.Dewpoint || 'Dewpoint');

    var tank1Instrument = document.getElementById('tank1Instrument');
    var tank2Instrument = document.getElementById('tank2Instrument');
    if (tank1Instrument) {
        tank1Instrument.setAttribute('aria-label', tankName + ' 1 level');
    }
    if (tank2Instrument) {
        tank2Instrument.setAttribute('aria-label', tankName + ' 2 level');
    }
}

function updateSensorPage(myObj) {
    var mainPowerMode = myObj.Device.MeasuringValues.MainPowerMode || myObj.Device.MeasuringValues.StandbyInputState;
    var mainPowerPin = myObj.Device.MeasuringValues.MainPowerInputPin || myObj.Device.MeasuringValues.StandbyInputPin;
    var mainPowerLevel = myObj.Device.MeasuringValues.MainPowerInputLevel || myObj.Device.MeasuringValues.StandbyInputLevel;
    setElementValue('quality', myObj.Device.NetworkParameter.ConnectionQuality.Value);
    updateSensorNames(myObj);
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
    setElementValue('standbyInputState', mainPowerMode.Value);
    setElementValue('standbyInputPin', mainPowerPin.Value);
    setElementValue('standbyInputLevel', mainPowerLevel.Value);
    setElementValue('standbyModeDiag', myObj.Device.MeasuringValues.standbyMode.Value);
    setElementValue('standbyInputStateDiag', mainPowerMode.Value);
    setElementValue('standbyInputPinDiag', mainPowerPin.Value);
    setElementValue('standbyInputLevelDiag', mainPowerLevel.Value);

    updateSensorIndicators(myObj);
    updateBatteryGauge(
        myObj.Device.MeasuringValues.BatteryVoltage.Value,
        myObj.Device.MeasuringValues.BatteryCapacity.Value,
        myObj.Device.MeasuringValues.BatteryAdc.Value
    );
    updateTemperatureGauge(myObj);
    updateTankCard('tank1', myObj.Device.MeasuringValues.Tank1.Value, myObj.Device.MeasuringValues.Tank1adc.Value);
    updateTankCard('tank2', myObj.Device.MeasuringValues.Tank2.Value, myObj.Device.MeasuringValues.Tank2adc.Value);
}

function refreshSensorPage() {
    fetchJson('/data.json', updateSensorPage);
}

document.addEventListener('DOMContentLoaded', function () {
    fetchJson('/staticdata.json', function (myObj) {
        staticData = myObj;
        refreshSensorPage();
    });
    startVisiblePolling(refreshSensorPage, 5000);
});
