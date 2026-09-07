#pragma once
#include <math.h>
#include <limits.h>
inline bool validTemperatureC(float value) {
  return isfinite(value) && value >= -55.0f && value <= 125.0f;
}

inline long telemetryRound(float value) {
  if (!isfinite(value)) return 0;
  if (static_cast<double>(value) >= static_cast<double>(LONG_MAX)) return LONG_MAX;
  if (static_cast<double>(value) <= static_cast<double>(LONG_MIN)) return LONG_MIN;
  return lroundf(value);
}
