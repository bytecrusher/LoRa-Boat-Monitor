const assert = require('assert');
const {execFileSync} = require('child_process');
const {decodeUplink} = require('../src/Payload_Formats_TTN_V3.js');
const context = require('../src/Payload_Formats_Ubidots.js');
const native = process.argv[2];
assert(native, 'Pass the compiled telemetry_payload_test executable.');
const packets = execFileSync(native, {encoding: 'utf8'}).trim().split('\n').map(line => {
  const [mask, hex] = line.split(' ');
  return {mask: Number(mask), bytes: [...Buffer.from(hex, 'hex')]};
});
const fields = [[64, 'voltage', 2.826], [1, 'tempbattery', 0], [2, 'temperature', 102.7],
  [4, 'speed', 79.66], [8, 'vedirectCurrent', -1], [16, 'tank1Adc', 6938], [32, 'standbyEpoch', 757869354]];
for (const {mask, bytes} of packets) {
  for (const decode of [b => decodeUplink({fPort: 3, bytes: b}), b => context.decodeUplink(b, 3)]) {
    const result = decode(bytes);
    assert(!result.errors, JSON.stringify(result));
    assert.equal(result.data.macAddress, '94:B9:7E:FE:F5:40');
    assert.equal(result.data.payloadSchema, 4);
    for (const [bit, field, value] of fields) {
      assert.equal(Object.hasOwn(result.data, field), Boolean(mask & bit), field);
      if (mask & bit) assert.equal(result.data[field], value, field);
    }
    for (let length = 0; length < bytes.length; length++) assert(decode(bytes.slice(0, length)).errors);
    assert(decode([...bytes, 0]).errors);
    assert(decode([5, ...bytes.slice(1)]).errors);
    assert(decode([4, mask | 128, ...bytes.slice(2)]).errors);
  }
}
// Optional cross-repository check: decode the actual C++ encoder output in PHP as well.
if (process.argv[3]) {
  const php = 'require $argv[1]; $out=[]; foreach(json_decode(stream_get_contents(STDIN),true) as $p)'
    + '{$out[]=TtnPayloadDecoder::decodeKnownPayload(3,base64_encode(pack("C*",...$p["bytes"])));} echo json_encode($out);';
  const decoded = JSON.parse(execFileSync('php', ['-r', php, process.argv[3]], {input: JSON.stringify(packets), encoding: 'utf8'}));
  decoded.forEach((data, i) => assert.deepStrictEqual(data, decodeUplink({fPort: 3, bytes: packets[i].bytes}).data));
}
console.log(`Compact payload: ${packets.length} C++ packets, both JS decoders, malformed inputs and optional PHP parity passed.`);
