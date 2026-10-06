#pragma once
#include <stdint.h>
#include <stddef.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdio>
#define HIGH 1
#define LOW 0
#define INPUT 0
#define INPUT_PULLUP 2
#define OUTPUT 1
#define HEX 16
inline uint32_t fakeMillis = 0;
inline int fakeButton = HIGH;
inline int fakePir = LOW;
inline uint32_t millis() { return fakeMillis; }
inline uint32_t micros() { return fakeMillis * 1000; }
inline void delay(unsigned long) {}
inline void pinMode(int,int) {}
inline void digitalWrite(int,int) {}
inline int digitalRead(int pin) { return pin == 27 ? fakeButton : pin == 26 ? fakePir : LOW; }
inline int analogRead(int) { return 1200; }
inline void randomSeed(int) {}
inline long random(long low, long) { return low; }
inline void tone(int,int) {}
inline void noTone(int) {}
using std::min; using std::max; using std::isnan;
template <class T> T constrain(T value, T low, T high) { return std::max(low, std::min(value, high)); }
struct SerialStub {
  void begin(int) {}
  template<class... T> void print(T...) {}
  template<class... T> void println(T...) {}
  template<class... T> void printf(T...) {}
};
inline SerialStub Serial;
