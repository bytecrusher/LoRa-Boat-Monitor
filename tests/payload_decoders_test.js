const assert = require('assert');
const { decodeUplink } = require('../src/Payload_Formats_TTN_V3.js');

function putUint16(bytes, offset, value) {
  bytes[offset] = value & 0xff;
  bytes[offset + 1] = (value >>> 8) & 0xff;
}

function putUint32(bytes, offset, value) {
  bytes[offset] = value & 0xff;
  bytes[offset + 1] = (value >>> 8) & 0xff;
  bytes[offset + 2] = (value >>> 16) & 0xff;
  bytes[offset + 3] = (value >>> 24) & 0xff;
}

const measurements = new Array(50).fill(0);
putUint16(measurements, 10, 12750);
putUint16(measurements, 22, 7525);
putUint16(measurements, 24, 4050);
measurements[26] = 0x21;
measurements.splice(27, 6, 0x94, 0xb9, 0x7e, 0xfe, 0xf5, 0x40);
measurements[33] = 88;
putUint16(measurements, 34, 3621);
putUint16(measurements, 36, 3673);
putUint16(measurements, 38, 425);
putUint16(measurements, 40, 12345);
putUint16(measurements, 42, 123);
putUint16(measurements, 44, 1310);
putUint16(measurements, 46, 0xff9c);
putUint16(measurements, 48, 2150);

const dynamicResult = decodeUplink({ fPort: 1, bytes: measurements }).data;
assert.strictEqual(dynamicResult.payloadType, 'measurements');
assert.strictEqual(dynamicResult.payloadSchema, 2);
assert.strictEqual(dynamicResult.voltage, 12.75);
assert.strictEqual(dynamicResult.level1, 75.25);
assert.strictEqual(dynamicResult.level2, 40.5);
assert.strictEqual(dynamicResult.alarm1, 1);
assert.strictEqual(dynamicResult.mainPowerOn, 1);
assert.strictEqual(dynamicResult.relay, 2);
assert.strictEqual(dynamicResult.batteryCapacity, 88);
assert.strictEqual(dynamicResult.tank1Adc, 3621);
assert.strictEqual(dynamicResult.speed, 4.25);
assert.strictEqual(dynamicResult.course, 123.45);
assert.strictEqual(dynamicResult.altitude, 12.3);
assert.strictEqual(dynamicResult.vedirectCurrent, -1);
assert.strictEqual(dynamicResult.macAddress, '94:B9:7E:FE:F5:40');

const schema3 = new Array(51).fill(0);
schema3[0] = 3;
putUint16(schema3, 1, 321);
putUint16(schema3, 3, 215);
putUint16(schema3, 5, 10203);
schema3[7] = 54;
putUint16(schema3, 8, 118);
putUint16(schema3, 10, 13625);
putUint16(schema3, 12, 204);
putUint32(schema3, 14, (-6796773) >>> 0);
putUint32(schema3, 18, 51193901);
schema3[22] = 75;
schema3[23] = 41;
schema3[24] = 0xe5;
schema3[25] = 92;
putUint16(schema3, 26, 3621);
putUint16(schema3, 28, 3673);
putUint16(schema3, 30, 425);
putUint16(schema3, 32, 12345);
putUint16(schema3, 34, 123);
putUint16(schema3, 36, 1310);
putUint16(schema3, 38, 0xff9c);
putUint16(schema3, 40, 2150);
putUint32(schema3, 42, 1781517600);
putUint32(schema3, 46, 1781518500);
schema3[50] = 0x31;

const schema3Result = decodeUplink({ fPort: 1, bytes: schema3 }).data;
assert.strictEqual(schema3Result.payloadSchema, 3);
assert.strictEqual(schema3Result.counter, 321);
assert.strictEqual(schema3Result.voltage, 13.625);
assert.strictEqual(schema3Result.longitude, -6.796773);
assert.strictEqual(schema3Result.latitude, 51.193901);
assert.strictEqual(schema3Result.wakeupEventPresent, true);
assert.strictEqual(schema3Result.standbyCause, 'Sleep standby');
assert.strictEqual(schema3Result.wakeupCause, 'Wakeup Timer');
assert.strictEqual(schema3Result.standbyEpoch, 1781517600);
assert.strictEqual(schema3Result.wakeupEpoch, 1781518500);
assert.strictEqual(schema3Result.mainPowerOn, 1);
assert.strictEqual(schema3Result.alarm1, schema3Result.mainPowerOn);

const device = new Array(34).fill(0);
device[0] = 1;
device[1] = 0x3f;
device[2] = 18;
device[3] = 5;
putUint16(device, 4, 15);
device[6] = 24;
device[7] = 1;
device[8] = 3;
device[9] = 10;
device[10] = 8;
device[12] = 1;
device[13] = 2;
'V1.15u'.split('').forEach((character, index) => { device[14 + index] = character.charCodeAt(0); });
device.splice(24, 6, 0x94, 0xb9, 0x7e, 0xfe, 0xf5, 0x40);

const deviceResult = decodeUplink({ fPort: 2, bytes: device }).data;
assert.strictEqual(deviceResult.payloadType, 'deviceConfig');
assert.strictEqual(deviceResult.firmwareVersion, 'V1.15u');
assert.strictEqual(deviceResult.standbyEnabled, true);
assert.strictEqual(deviceResult.transmitPriority, 'WifiFirst');
assert.strictEqual(deviceResult.loraOperationMode, 'Always');
assert.strictEqual(deviceResult.environmentSensor, 'VEdirect-Read');
assert.strictEqual(deviceResult.macAddress, '94:B9:7E:FE:F5:40');

console.log('Payload decoder tests passed');
