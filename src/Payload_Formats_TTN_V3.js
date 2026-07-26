// TTN V3 application uplink formatter for LoRa Boat Monitor.
// FPort 1 carries recurring measurements (33-byte legacy, 50-byte schema 2,
// or 51-byte schema 3 with explicit standby/wakeup event data).
// FPort 2 carries the 34-byte device/configuration schema and no credentials.
// Keep this file synchronized with the application-level uplink formatter in TTN.

function decodeUplink(input) {
  if (input.fPort === 2) {
    return decodeDevicePayload(input.bytes);
  }
  if (input.fPort === 1 && input.bytes.length === 51 && input.bytes[0] === 3) {
    return decodeMeasurementPayloadV3(input.bytes);
  }

  var data = {};
  var events = {
    1: "setup",
    2: "interval",
    3: "motion",
    4: "button"
  };
//  data.event = events[input.fPort];

  var voffset = 0;      // Voltage offset
  var toffset = 0;      // Temperature offset for BME280
  var poffset = 0;      // Pressure offset for altitude

  data.payloadType = "measurements";
  data.payloadSchema = input.bytes.length >= 50 ? 2 : 1;
  data.counter = readUint16(input.bytes, 0);
  var temperature = (((input.bytes[3] << 8) | input.bytes[2]) / 100) - 50 + toffset;
  data.temperature = Math.round(temperature * 10) / 10;
  data.pressure = ((input.bytes[5] << 8) | input.bytes[4]) / 10 + poffset;
  data.humidity = ((input.bytes[7] << 8) | input.bytes[6]) / 100;
  var dewpoint = (((input.bytes[9] << 8) | input.bytes[8]) / 100) - 50;
  data.dewpoint = Math.round(dewpoint * 10) / 10;
  var voltage = ((input.bytes[11] << 8) | input.bytes[10]) / 1000 + voffset;
  data.voltage = Math.round(voltage * 1000) / 1000;
  var tempbattery = (((input.bytes[13] << 8) | input.bytes[12]) / 100) - 50;
  data.tempbattery = Math.round(tempbattery * 10) / 10;
  var longitude = ((input.bytes[15] << 8) | input.bytes[14]) / 100 + ((input.bytes[17] << 8) | input.bytes[16]) / 1000000;
  data.longitude = longitude;
  var latitude = ((input.bytes[19] << 8) | input.bytes[18]) / 100 + ((input.bytes[21] << 8) | input.bytes[20]) / 1000000;
  data.latitude = latitude;
  data.altitude = 1;
  data.hdop = 1.1;
  data.position = {"value": 0, context:{"lat": latitude, "lng": longitude}};
  data.level1 = readUint16(input.bytes, 22) / 100;
  data.level2 = readUint16(input.bytes, 24) / 100;
  data.alarm1 = input.bytes[26] & 0x01;
  data.environmentPresent = (input.bytes[26] & 0x04) !== 0;
  data.vedirectPresent = (input.bytes[26] & 0x08) !== 0;
  data.relay = (input.bytes[26] >> 4) & 0x03;
  data.gpsFix = (input.bytes[26] & 0x40) !== 0;
  if (input.bytes.length >= 33) {
    data.macAddress = formatMacAddress(input.bytes, 27);
  }
  if (input.bytes.length >= 50) {
    data.batteryCapacity = input.bytes[33];
    data.tank1Adc = readUint16(input.bytes, 34);
    data.tank2Adc = readUint16(input.bytes, 36);
    data.speed = readUint16(input.bytes, 38) / 100;
    data.course = readUint16(input.bytes, 40) / 100;
    data.altitude = readInt16(input.bytes, 42) / 10;
    data.vedirectVoltage = readUint16(input.bytes, 44) / 100;
    data.vedirectCurrent = readInt16(input.bytes, 46) / 100;
    data.vedirectTemperature = readInt16(input.bytes, 48) / 100;
  }

  var warnings = [];
  if (data.voltage < 10) {
    warnings.push("Battery undervolteage");
  }
  if (data.voltage > 14.7) {
    warnings.push("Battery overload");
  }
  return {
    data: data,
    warnings: warnings
  };
}

function decodeMeasurementPayloadV3(bytes) {
  var status = bytes[24];
  var longitude = readInt32(bytes, 14) / 1000000;
  var latitude = readInt32(bytes, 18) / 1000000;
  var causeCodes = bytes[50];
  var standbyCauses = ["", "Sleep standby"];
  var wakeupCauses = ["", "Wakeup EXT0", "Wakeup EXT1", "Wakeup Timer", "Wakeup Touch", "Wakeup ULP", "Wakeup Other"];
  var standbyCode = causeCodes & 0x0f;
  var wakeupCode = (causeCodes >> 4) & 0x0f;
  var voltage = readUint16(bytes, 10) / 1000;

  var data = {
    payloadType: "measurements",
    payloadSchema: 3,
    counter: readUint16(bytes, 1),
    temperature: readInt16(bytes, 3) / 10,
    pressure: readUint16(bytes, 5) / 10,
    humidity: bytes[7],
    dewpoint: readInt16(bytes, 8) / 10,
    voltage: voltage,
    tempbattery: readInt16(bytes, 12) / 10,
    longitude: longitude,
    latitude: latitude,
    position: { value: 0, context: { lat: latitude, lng: longitude } },
    level1: bytes[22],
    level2: bytes[23],
    alarm1: status & 0x01,
    environmentPresent: (status & 0x04) !== 0,
    vedirectPresent: (status & 0x08) !== 0,
    relay: (status >> 4) & 0x03,
    gpsFix: (status & 0x40) !== 0,
    wakeupEventPresent: (status & 0x80) !== 0,
    batteryCapacity: bytes[25],
    tank1Adc: readUint16(bytes, 26),
    tank2Adc: readUint16(bytes, 28),
    speed: readUint16(bytes, 30) / 100,
    course: readUint16(bytes, 32) / 100,
    altitude: readInt16(bytes, 34) / 10,
    vedirectVoltage: readUint16(bytes, 36) / 100,
    vedirectCurrent: readInt16(bytes, 38) / 100,
    vedirectTemperature: readInt16(bytes, 40) / 100,
    standbyEpoch: readUint32(bytes, 42),
    wakeupEpoch: readUint32(bytes, 46),
    standbyCause: standbyCauses[standbyCode] || (standbyCode ? "Standby Other" : ""),
    wakeupCause: wakeupCauses[wakeupCode] || (wakeupCode ? "Wakeup Other" : "")
  };

  var warnings = [];
  if (voltage < 10) warnings.push("Battery undervoltage");
  if (voltage > 14.7) warnings.push("Battery overload");
  if (data.wakeupEventPresent && (!data.standbyEpoch || !data.wakeupEpoch)) {
    warnings.push("Wakeup event flag is set but a timestamp is missing");
  }
  return { data: data, warnings: warnings };
}

function decodeDevicePayload(bytes) {
  if (bytes.length < 34 || bytes[0] !== 1) {
    return { data: {}, errors: ["Unsupported device payload"] };
  }

  var flags = bytes[1];
  var firmwareBytes = bytes.slice(14, 22);
  var firmwareVersion = "";
  for (var i = 0; i < firmwareBytes.length && firmwareBytes[i] !== 0; i++) {
    firmwareVersion += String.fromCharCode(firmwareBytes[i]);
  }

  return {
    data: {
      payloadType: "deviceConfig",
      payloadSchema: bytes[0],
      standbyEnabled: (flags & 0x01) !== 0,
      wifiDuringStandby: (flags & 0x02) !== 0,
      wifiUploadEnabled: (flags & 0x04) !== 0,
      dynamicSpreadingFactor: (flags & 0x08) !== 0,
      mdnsEnabled: (flags & 0x10) !== 0,
      webAuthenticationEnabled: (flags & 0x20) !== 0,
      firmwareChannel: (flags & 0x40) !== 0 ? "stable" : "beta",
      configVersion: bytes[2],
      transmitIntervalMinutes: bytes[3],
      standbySleepMinutes: readUint16(bytes, 4),
      autoUpdateIntervalHours: bytes[6],
      transmitPriority: bytes[7] === 1 ? "WifiFirst" : "LoRaFirst",
      loraOperationMode: ["Off", "Standby", "PowerOn", "Always"][bytes[8]] || "Unknown",
      spreadingFactor: bytes[9],
      loraChannel: bytes[10],
      serverMode: bytes[11],
      temperatureSensor: bytes[12] === 1 ? "DS18B20" : "Off",
      environmentSensor: ["Off", "BME280", "VEdirect-Read", "VEdirect-Send"][bytes[13]] || "Unknown",
      firmwareVersion: firmwareVersion,
      deviceId: bytes[22],
      relayMode: bytes[23],
      macAddress: formatMacAddress(bytes, 24),
      configHash: readUint32(bytes, 30).toString(16).toUpperCase()
    }
  };
}

function readUint16(bytes, offset) {
  return ((bytes[offset + 1] || 0) << 8) | (bytes[offset] || 0);
}

function readInt16(bytes, offset) {
  var value = readUint16(bytes, offset);
  return value & 0x8000 ? value - 0x10000 : value;
}

function readUint32(bytes, offset) {
  return ((bytes[offset] || 0) |
    ((bytes[offset + 1] || 0) << 8) |
    ((bytes[offset + 2] || 0) << 16) |
    ((bytes[offset + 3] || 0) << 24)) >>> 0;
}

function readInt32(bytes, offset) {
  return readUint32(bytes, offset) | 0;
}

function formatMacAddress(bytes, offset) {
  var parts = [];
  for (var i = 0; i < 6; i++) {
    var part = (bytes[offset + i] || 0).toString(16).toUpperCase();
    if (part.length < 2) {
      part = "0" + part;
    }
    parts.push(part);
  }
  return parts.join(":");
}

if (typeof module !== "undefined") {
  module.exports = { decodeUplink: decodeUplink };
}
