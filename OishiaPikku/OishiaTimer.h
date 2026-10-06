#pragma once
#include <stdint.h>

// Durations, not absolute deadlines. Expiration is latched until restarted,
// so an old animation cannot reappear when millis wraps.
class OishiaTimer {
 public:
  void start(uint32_t now, uint32_t duration) { at = now; period = duration; active = true; }
  void stop() { active = false; }
  bool running(uint32_t now) {
    if (active && uint32_t(now - at) >= period) active = false;
    return active;
  }
  bool due(uint32_t now) { return !running(now); }
 private:
  uint32_t at = 0, period = 0;
  bool active = false;
};
