#pragma once
#include "Arduino.h"
#include <map>
#include <vector>
#include <string>
struct Preferences {
  bool fail = false;
  int writes = 0;
  std::map<std::string,std::vector<uint8_t>> data;
  bool begin(const char *, bool) { return !fail; }
  bool getBool(const char *, bool fallback) { return fallback; }
  uint32_t getUInt(const char *key, uint32_t fallback) { return !strcmp(key,"pets") ? 42 : !strcmp(key,"visits") ? 17 : fallback; }
  size_t getBytesLength(const char *key) { return data[key].size(); }
  size_t getBytes(const char *key, void *out, size_t size) { if(data[key].size()!=size)return 0; memcpy(out,data[key].data(),size);return size; }
  size_t putBytes(const char *key, const void *value, size_t size) {
    ++writes; if(fail)return 0;
    const uint8_t *p = static_cast<const uint8_t *>(value); data[key]={p,p+size};return size;
  }
};
