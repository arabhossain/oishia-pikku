#pragma once
#include "Arduino.h"
#include <string>
using String = std::string;
namespace MockNetwork {
inline bool connected = false, waiting = false, blockWrites = false;
inline std::string input, output;
inline size_t largestWrite = 0;
inline void accept(std::string bytes) {
  connected = waiting = true; blockWrites = false; input = bytes; output.clear(); largestWrite = 0;
}
}
struct NetworkClient {
  bool owned = false;
  NetworkClient(bool value=false) : owned(value) {}
  explicit operator bool() const { return owned && MockNetwork::connected; }
  bool connected() const { return bool(*this); }
  void stop() { if (owned) MockNetwork::connected=false; owned=false; }
  int fd() const { return 1; }
};
