#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// Newline-delimited JSON. Oversized frames are discarded through the delimiter.
// A reconnect or timeout always starts with a clean parser.
class OishiaBleFrames {
 public:
  static constexpr size_t CAPACITY = 1536;
  enum Result { Waiting, Complete, Rejected };
  Result push(uint8_t byte) {
    if (byte == '\n') {
      bool invalid = dropping || length == 0;
      buffer[length] = 0;
      dropping = false;
      length = 0;
      return invalid ? Rejected : Complete;
    }
    if (dropping) return Waiting;
    if (byte == 0 || length == CAPACITY) { dropping = true; return Waiting; }
    buffer[length++] = char(byte);
    return Waiting;
  }
  void reset() { length = 0; dropping = false; buffer[0] = 0; }
  bool partial() const { return length || dropping; }
  const char *value() const { return buffer; }
 private:
  char buffer[CAPACITY + 1] = {};
  size_t length = 0;
  bool dropping = false;
};
