#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

inline bool loraConfirmationAllowed(int64_t now, int64_t last, bool withoutClock) {
  if (withoutClock) return false;
  if (now <= 1700000000) return last == 0;
  return last == 0 || (now >= last && now - last >= 3 * 60 * 60);
}

inline size_t telemetryLength(uint8_t mask) {
  static const uint8_t lengths[] = {2, 7, 14, 6, 6, 9, 3};
  size_t size = 11;
  for (uint8_t i = 0; i < 7; ++i) if (mask & (1 << i)) size += lengths[i];
  return size;
}

inline uint8_t fitTelemetryMask(uint8_t mask, uint8_t &rotation) {
  static const uint8_t optional[] = {2, 4, 8, 16};
  const uint8_t start = rotation % 4;
  // Never defer an event: a new sleep cycle may replace the pending pair.
  for (uint8_t i = 0; i < 4 && telemetryLength(mask) > 51; ++i) {
    const uint8_t index = (start + i) % 4;
    if (mask & optional[index]) {
      mask &= ~optional[index];
      rotation = (index + 1) % 4;
    }
  }
  return mask;
}

// FPort 3 separates compact packets from unversioned legacy counters on FPort 1.
inline size_t compactTelemetry(const uint8_t *legacy, const uint8_t *mac, uint8_t mask,
                               uint8_t *output, size_t capacity) {
  static const uint8_t bits[] = {64, 1, 2, 4, 8, 16, 32};
  static const uint8_t lengths[] = {3, 2, 7, 14, 6, 6, 9};
  static const uint8_t offsets[][14] = {
    {10, 11, 25}, {12, 13}, {3, 4, 5, 6, 7, 8, 9},
    {14, 15, 16, 17, 18, 19, 20, 21, 30, 31, 32, 33, 34, 35},
    {36, 37, 38, 39, 40, 41}, {22, 23, 26, 27, 28, 29},
    {42, 43, 44, 45, 46, 47, 48, 49, 50}
  };
  size_t length = 11;
  for (size_t i = 0; i < 7; ++i) if (mask & bits[i]) length += lengths[i];
  if ((mask & 128) || length > capacity || length > 51) return 0;
  output[0] = 4;
  output[1] = mask;
  memcpy(output + 2, mac, 6);
  output[8] = legacy[1];
  output[9] = legacy[2];
  output[10] = legacy[24] & 0x31;
  size_t cursor = 11;
  for (size_t i = 0; i < 7; ++i) {
    if (!(mask & bits[i])) continue;
    for (size_t j = 0; j < lengths[i]; ++j) output[cursor++] = legacy[offsets[i][j]];
  }
  return length;
}
