// This component is not for the Arduino code. The component is neccessary for implementation in the TTN network.
// The function Decoder() decode the received payload in the TTN network. Please copy the function in the TTN console
// under PAYLOAD FORMATS and save it.
// https://console.thethingsnetwork.org/applications/lora-testsensor/payload-formats

// For a data transmission from TTN V3 to Ubidots you need an Webhook in TTN.
// At fist create a The Things Stack Plugin in Ubitots.
// Then copy the following code under Decoding Function in Ubidots Decoder Settings

function format_payload(args){
  var ubidots_payload = {};
  // Log received data for debugging purposes:
  // console.log(JSON.stringify(args));
  // Get RSSI and SNR variables using gateways data:
  var gateways = args['uplink_message']['rx_metadata'];
  for (const i in gateways) {  
    // Get gateway EUI and name
    var gw = gateways[i];
    var gw_eui = gw['gateway_ids']['eui'];
    var gw_id = gw['gateway_ids']['gateway_id'];
    // Build RSSI and SNR variables
    ubidots_payload['rssi-' + gw_id] = {
      "value": gw['rssi'],
      "context": {
        "channel_index": gw['channel_index'],
        "channel_rssi": gw['channel_rssi'],
        "gw_eui": gw_eui,
        "gw_id": gw_id,
        "uplink_token": gw['uplink_token']
      }
    }
    ubidots_payload['snr-' + gw_id] = gw['snr'];
  }

  // Get Fcnt and Port variables:
  ubidots_payload['f_cnt'] = args['uplink_message']['f_cnt'];
  ubidots_payload['f_port'] = args['uplink_message']['f_port'];

  // Get uplink's timestamp
  ubidots_payload['timestamp'] = new Date(args['uplink_message']['received_at']).getTime(); 

  // If you're already decoding in TTS using payload formatters, 
  // then uncomment the following line to use "uplink_message.decoded_payload".
  // PROTIP: Make sure the incoming decoded payload is an Ubidots-compatible JSON (See https://ubidots.com/docs/hw/#sending-data)
  // var decoded_payload = args['uplink_message']['decoded_payload'];
 
  // By default, this plugin uses "uplink_message.frm_payload" and sends it to the decoding function "decodeUplink".
  // For more vendor-specific decoders, check out https://github.com/TheThingsNetwork/lorawan-devices/tree/master/vendor
  let bytes =  Buffer.from(args['uplink_message']['frm_payload'], 'base64');
  var decoded_payload = decodeUplink(bytes, args['uplink_message']['f_port'])['data'];
  // Merge decoded payload into Ubidots payload
  Object.assign(ubidots_payload, decoded_payload);
  // return ubidots_payload
  return decoded_payload
}

function decodeUplink(bytes, fPort) {
  if (fPort === 2) {
    return decodeDevicePayload(bytes);
  }
  if (fPort === 1 && bytes.length === 51 && bytes[0] === 3) {
    return decodeMeasurementPayloadV3(bytes);
  }
  // Decoder for the LoRa Boat Monitor
  var decoded = {};
  
  // If received data is of Environment Monitoring type
  var voffset = 0;      // Voltage offset
  var toffset = 0;      // Temperature offset for BME280
  var poffset = 0;      // pressure offset for altitude
  decoded.payloadType = "measurements";
  decoded.payloadSchema = bytes.length >= 50 ? 2 : 1;
  decoded.counter = readUint16(bytes, 0);
  var temperature = (((bytes[3] << 8) | bytes[2]) / 100) - 50 + toffset;
//  decoded.temperature = Math.round(temperature * 10) / 10;
  decoded.pressure = ((bytes[5] << 8) | bytes[4]) / 10 + poffset;
  decoded.humidity = ((bytes[7] << 8) | bytes[6]) / 100;
  var dewpoint = (((bytes[9] << 8) | bytes[8]) / 100) - 50;
  decoded.dewpoint = Math.round(dewpoint * 10) / 10;
  var voltage = ((bytes[11] << 8) | bytes[10]) / 1000 + voffset;
  decoded.voltage = Math.round(voltage * 1000) / 1000;
  var tempbattery = (((bytes[13] << 8) | bytes[12]) / 100) - 50;
  decoded.tempbattery = Math.round(tempbattery * 10) / 10;
  decoded.longitude = ((bytes[15] << 8) | bytes[14]) / 100 + ((bytes[17] << 8) | bytes[16]) / 1000000;
  decoded.latitude = ((bytes[19] << 8) | bytes[18]) / 100 + ((bytes[21] << 8) | bytes[20]) / 1000000;
  decoded.level1 = readUint16(bytes, 22) / 100;
  decoded.level2 = readUint16(bytes, 24) / 100;
  decoded.alarm1 = bytes[26] & 0x01;
  decoded.environmentPresent = (bytes[26] & 0x04) !== 0;
  decoded.vedirectPresent = (bytes[26] & 0x08) !== 0;
  if (bytes.length >= 33) {
    decoded.macAddress = formatMacAddress(bytes, 27);
  }
  decoded.relay = (bytes[26] >> 4) & 0x03;
  decoded.gpsFix = (bytes[26] & 0x40) !== 0;
  if (bytes.length >= 50) {
    decoded.batteryCapacity = bytes[33];
    decoded.tank1Adc = readUint16(bytes, 34);
    decoded.tank2Adc = readUint16(bytes, 36);
    decoded.speed = readUint16(bytes, 38) / 100;
    decoded.course = readUint16(bytes, 40) / 100;
    decoded.altitude = readInt16(bytes, 42) / 10;
    decoded.vedirectVoltage = readUint16(bytes, 44) / 100;
    decoded.vedirectCurrent = readInt16(bytes, 46) / 100;
    decoded.vedirectTemperature = readInt16(bytes, 48) / 100;
  }
  return {"data": decoded};
}

function decodeMeasurementPayloadV3(bytes) {
  var status = bytes[24];
  var causeCodes = bytes[50];
  var standbyCauses = ["", "Sleep standby"];
  var wakeupCauses = ["", "Wakeup EXT0", "Wakeup EXT1", "Wakeup Timer", "Wakeup Touch", "Wakeup ULP", "Wakeup Other"];
  var standbyCode = causeCodes & 0x0f;
  var wakeupCode = (causeCodes >> 4) & 0x0f;
  return {"data": {
    "payloadType": "measurements",
    "payloadSchema": 3,
    "counter": readUint16(bytes, 1),
    "temperature": readInt16(bytes, 3) / 10,
    "pressure": readUint16(bytes, 5) / 10,
    "humidity": bytes[7],
    "dewpoint": readInt16(bytes, 8) / 10,
    "voltage": readUint16(bytes, 10) / 1000,
    "tempbattery": readInt16(bytes, 12) / 10,
    "longitude": readInt32(bytes, 14) / 1000000,
    "latitude": readInt32(bytes, 18) / 1000000,
    "level1": bytes[22],
    "level2": bytes[23],
    "alarm1": status & 0x01,
    "environmentPresent": (status & 0x04) !== 0,
    "vedirectPresent": (status & 0x08) !== 0,
    "relay": (status >> 4) & 0x03,
    "gpsFix": (status & 0x40) !== 0,
    "wakeupEventPresent": (status & 0x80) !== 0,
    "batteryCapacity": bytes[25],
    "tank1Adc": readUint16(bytes, 26),
    "tank2Adc": readUint16(bytes, 28),
    "speed": readUint16(bytes, 30) / 100,
    "course": readUint16(bytes, 32) / 100,
    "altitude": readInt16(bytes, 34) / 10,
    "vedirectVoltage": readUint16(bytes, 36) / 100,
    "vedirectCurrent": readInt16(bytes, 38) / 100,
    "vedirectTemperature": readInt16(bytes, 40) / 100,
    "standbyEpoch": readUint32(bytes, 42),
    "wakeupEpoch": readUint32(bytes, 46),
    "standbyCause": standbyCauses[standbyCode] || (standbyCode ? "Standby Other" : ""),
    "wakeupCause": wakeupCauses[wakeupCode] || (wakeupCode ? "Wakeup Other" : "")
  }};
}

function decodeDevicePayload(bytes) {
  if (bytes.length < 34 || bytes[0] !== 1) return {"data": {"payloadType": "unsupported"}};
  var firmwareVersion = "";
  for (var i = 14; i < 22 && bytes[i] !== 0; i++) firmwareVersion += String.fromCharCode(bytes[i]);
  return {"data": {
    "payloadType": "deviceConfig",
    "payloadSchema": bytes[0],
    "firmwareVersion": firmwareVersion,
    "firmwareChannel": (bytes[1] & 0x40) !== 0 ? "stable" : "beta",
    "standbyEnabled": (bytes[1] & 0x01) !== 0,
    "transmitIntervalMinutes": bytes[3],
    "standbySleepMinutes": readUint16(bytes, 4),
    "macAddress": formatMacAddress(bytes, 24),
    "configHash": readUint32(bytes, 30).toString(16).toUpperCase()
  }};
}

function readUint16(bytes, offset) {
  return ((bytes[offset + 1] || 0) << 8) | (bytes[offset] || 0);
}

function readInt16(bytes, offset) {
  var value = readUint16(bytes, offset);
  return value & 0x8000 ? value - 0x10000 : value;
}

function readUint32(bytes, offset) {
  return ((bytes[offset] || 0) | ((bytes[offset + 1] || 0) << 8) |
    ((bytes[offset + 2] || 0) << 16) | ((bytes[offset + 3] || 0) << 24)) >>> 0;
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

module.exports = { format_payload, decodeUplink };
