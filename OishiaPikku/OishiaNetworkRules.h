#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace OishiaNetworkRules {
inline bool validPin(const char *pin, size_t length) {
  if (!pin || length != 4) return false;
  for (size_t i = 0; i < length; ++i) if (pin[i] < '0' || pin[i] > '9') return false;
  return true;
}
constexpr uint32_t CONNECT_TIMEOUT_MS = 20000;
constexpr uint32_t RETRY_INTERVAL_MS = 30000;
constexpr uint32_t SETUP_GRACE_MS = 60000;
constexpr uint32_t MANUAL_SETUP_MS = 300000;

inline bool elapsed(uint32_t now, uint32_t since, uint32_t duration) {
  return uint32_t(now - since) >= duration;
}

inline bool printable(const char *text, size_t length) {
  for (size_t i = 0; i < length; ++i) {
    if (uint8_t(text[i]) < 32 || uint8_t(text[i]) > 126) return false;
  }
  return true;
}

inline bool validMessage(const char *text, size_t length) {
  if (!text || length == 0 || length > 21 || !printable(text, length)) return false;
  for (size_t i = 0; i < length; ++i) if (text[i] != ' ') return true;
  return false;
}

inline bool validCredentials(const char *ssid, size_t ssidLength,
                             const char *password, size_t passwordLength) {
  if (!ssid || !password || ssidLength == 0 || ssidLength > 32 || passwordLength > 64) return false;
  // SSIDs may contain UTF-8 and spaces, but never NUL or control characters.
  for (size_t i = 0; i < ssidLength; ++i) if (uint8_t(ssid[i]) < 32 || uint8_t(ssid[i]) == 127) return false;
  if (passwordLength == 0) return true;  // Explicitly allow an open home network.
  if (passwordLength < 8 || !printable(password, passwordLength)) return false;
  if (passwordLength == 64) {
    for (size_t i = 0; i < passwordLength; ++i) {
      char c = password[i];
      if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return false;
    }
  }
  return true;
}
}  // namespace OishiaNetworkRules
