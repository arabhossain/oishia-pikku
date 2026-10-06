
#include "DHT11.h"
#include <Arduino.h>

bool DHT_nonblocking_sensor::measure(float *temperature, float *humidity) {
    static unsigned long measurement_timestamp = millis();

    /* Measure once every four seconds. */
    if (millis() - measurement_timestamp > 3000ul) {
        if (dht_sensor.measure(temperature, humidity) == true) {
            measurement_timestamp = millis();
            return true;
        }
    }

    return false;
}