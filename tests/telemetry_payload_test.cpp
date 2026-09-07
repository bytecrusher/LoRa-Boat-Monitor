#include "../src/TelemetryPayload.h"
#include "../src/TelemetryValidity.h"
#include <assert.h>
#include <stdio.h>

int main() {
  assert(loraConfirmationAllowed(0, 0, false));
  assert(!loraConfirmationAllowed(0, 0, true));
  assert(!loraConfirmationAllowed(0, 1800000000, false));
  assert(loraConfirmationAllowed(1800000000, 0, false));
  assert(!loraConfirmationAllowed(1800000000, 0, true));
  assert(!loraConfirmationAllowed(1799999999, 1800000000, false));
  assert(!loraConfirmationAllowed(1800010799, 1800000000, false));
  assert(loraConfirmationAllowed(1800010800, 1800000000, false));
  assert(validTemperatureC(0));
  assert(validTemperatureC(-55));
  assert(validTemperatureC(125));
  assert(!validTemperatureC(-127));
  assert(!validTemperatureC(NAN));
  assert(!validTemperatureC(INFINITY));
  assert(telemetryRound(NAN) == 0);
  assert(telemetryRound(-12.5f) == -13);
  const uint8_t mac[] = {0x94,0xb9,0x7e,0xfe,0xf5,0x40};
  uint8_t legacy[51];
  for (int i = 0; i < 51; ++i) legacy[i] = i;
  legacy[12] = legacy[13] = 0; // Real zero Celsius is a valid sample.
  legacy[38] = 0x9c; legacy[39] = 0xff; // Signed VE current: -1 A.
  uint8_t output[52];
  for (int mask = 0; mask < 128; ++mask) {
    uint8_t rotation = 0, observed = 0;
    for (int attempt = 0; attempt < 8; ++attempt) {
      const uint8_t fitted = fitTelemetryMask(mask, rotation);
      assert(telemetryLength(fitted) <= 51);
      assert((fitted & (32 | 64 | 1)) == (mask & (32 | 64 | 1)));
      observed |= fitted;
    }
    assert(observed == mask); // No enabled group is starved by repeated events.
  }
  for (int mask = 0; mask < 128; ++mask) {
    memset(output, 0xee, sizeof(output));
    size_t size = compactTelemetry(legacy, mac, mask, output, 51);
    assert(output[51] == 0xee);
    if (size == 0) continue;
    assert(size >= 11 && size <= 51);
    assert(output[0] == 4 && output[1] == mask);
    assert(memcmp(output + 2, mac, 6) == 0);
    printf("%d ", mask);
    for (size_t i = 0; i < size; ++i) printf("%02x", output[i]);
    printf("\n");
  }
  assert(compactTelemetry(legacy, mac, 127, output, 51) == 0);
  assert(compactTelemetry(legacy, mac, 128, output, 51) == 0);
  assert(compactTelemetry(legacy, mac, 0, output, 10) == 0);
}
