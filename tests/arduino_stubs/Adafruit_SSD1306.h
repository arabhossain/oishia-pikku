#pragma once
#include "Arduino.h"
#define SSD1306_WHITE 1
#define SSD1306_BLACK 0
#define SSD1306_SWITCHCAPVCC 0
#define SSD1306_DISPLAYOFF 0xAE
#define SSD1306_DISPLAYON 0xAF
#define SSD1306_SETCONTRAST 0x81
struct Adafruit_SSD1306 {
  template<class... T> Adafruit_SSD1306(T...) {}
  bool begin(int,int) { return true; }
  void ssd1306_command(uint8_t) {}
  void clearDisplay() {} void display() {}
  void getTextBounds(const char*, int, int, int16_t *x, int16_t *y, uint16_t *w, uint16_t *h) { *x=*y=0; *w=*h=8; }
  template<class... T> void drawCircle(T...) {}
  template<class... T> void drawLine(T...) {}
  template<class... T> void drawPixel(T...) {}
  template<class... T> void drawRoundRect(T...) {}
  template<class... T> void fillCircle(T...) {}
  template<class... T> void fillRect(T...) {}
  template<class... T> void fillRoundRect(T...) {}
  template<class... T> void fillTriangle(T...) {}
  template<class... T> void print(T...) {}
  template<class... T> void setCursor(T...) {}
  template<class... T> void setTextColor(T...) {}
  template<class... T> void setTextSize(T...) {}
};
