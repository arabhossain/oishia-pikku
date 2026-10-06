#pragma once
#include <stdint.h>
#include <string.h>
#include "OishiaTimer.h"

// Owned exclusively by the network task. HTTP and BLE share the same PIN
// budget. Tokens are random 128-bit hex strings supplied by esp_fill_random.
class OishiaAuth {
 public:
  static constexpr uint32_t SESSION_IDLE_MS = 1800000;
  static constexpr uint32_t SESSION_MAX_MS = 28800000;
  void open(uint32_t now) { window.start(now, 300000); cooldown.stop(); failures = 0; }
  void tick(uint32_t now) { for (auto &s : sessions) expire(s, now); }
  bool commissioning(uint32_t now) { return window.running(now); }
  int checkPin(const char *supplied, const char *pin, uint32_t now) {
    if (!commissioning(now)) return 403;
    if (cooldown.running(now)) return 429;
    if (!equal(supplied, pin, 4)) {
      if (++failures >= 5) { failures = 0; cooldown.start(now, 60000); }
      return 401;
    }
    return 200;
  }
  bool issue(const char *token, uint32_t now) {
    for (auto &s : sessions) {
      expire(s, now);
      if (!s.token[0]) { strcpy(s.token, token); s.created = s.used = now; return true; }
    }
    return false;
  }
  bool authorize(const char *token, uint32_t now) {
    for (auto &s : sessions) {
      expire(s, now);
      if (s.token[0] && equal(token, s.token, 32)) { s.used = now; return true; }
    }
    return false;
  }
  void revoke(const char *token) { for (auto &s : sessions) if (equal(token, s.token, 32)) s = {}; }
  void reset(uint32_t now) { for (auto &s : sessions) s = {}; open(now); }
 private:
  struct Session { char token[33] = {}; uint32_t created = 0, used = 0; } sessions[4];
  OishiaTimer window, cooldown;
  uint8_t failures = 0;
  static bool equal(const char *a, const char *b, size_t n) {
    if (!a || !b || strlen(a) != n || strlen(b) != n) return false;
    uint8_t difference = 0;
    for (size_t i = 0; i < n; ++i) difference |= a[i] ^ b[i];
    return difference == 0;
  }
  static void expire(Session &s, uint32_t now) {
    if (uint32_t(now - s.used) >= SESSION_IDLE_MS || uint32_t(now - s.created) >= SESSION_MAX_MS) s = {};
  }
};
