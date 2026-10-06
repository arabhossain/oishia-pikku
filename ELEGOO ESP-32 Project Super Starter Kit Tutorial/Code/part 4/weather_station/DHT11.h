// DHT11.h
#ifndef DHT11_H
#define DHT11_H

#include <dht_nonblocking.h>

#define DHT_SENSOR_TYPE DHT_TYPE_11
#define DHT_SENSOR_PIN 26

class DHT_nonblocking_sensor {
public:
    DHT_nonblocking dht_sensor;

    DHT_nonblocking_sensor() : dht_sensor(DHT_SENSOR_PIN, DHT_SENSOR_TYPE) {}

    bool measure(float *temperature, float *humidity);
};

#endif // DHT11_H