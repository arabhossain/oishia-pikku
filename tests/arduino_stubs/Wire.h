#pragma once
#include "Arduino.h"
struct WireStub {
  bool fail = false;
  void begin(int,int) {} void setTimeOut(int) {} void beginTransmission(int) {} void write(int) {}
  int endTransmission(bool=true) { return fail ? 1 : 0; }
  int requestFrom(uint8_t,uint8_t n) { return fail ? 0 : n; }
  int read() { return 0; }
};
inline WireStub Wire;
