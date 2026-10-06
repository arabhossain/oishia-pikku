#pragma once
#include <stdint.h>
#include <time.h>
#include "OishiaSettings.h"

inline bool oishiaClockTimeValid(time_t value) {
  // 2024-01-01. Earlier values mean SNTP has not completed after boot.
  return value >= 1704067200;
}

inline bool validOishiaBrowserEpoch(uint32_t value) {
  // Accept 2024-01-01 through 2100-01-01. This rejects missing, wrapped, and
  // obviously incorrect browser clocks before they reach settimeofday().
  return value >= 1704067200u && value <= 4102444800u;
}

inline bool shouldShowOishiaClock(const OishiaSettings &settings, uint32_t now,
                                  uint32_t lastPresenceAt, bool blocked) {
  return settings.clockWhenAway && !blocked &&
    uint32_t(now - lastPresenceAt) >= settings.clockAfterSeconds * 1000u;
}
