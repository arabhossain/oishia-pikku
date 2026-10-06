#pragma once
#include "Arduino.h"
#define DHT11 11
struct DHT { DHT(int,int) {} void begin() {} float readTemperature() { return 24; } float readHumidity() { return 50; } };
