#include "SHT40Driver.h"

Adafruit_SHT4x sht4;

bool detectSHT40(TwoWire* wire, uint8_t addr) {
  return sht4.begin(wire);
}

uint8_t addSHT40Sensors(I2CSensor* sensors, uint8_t max_sensors, uint8_t addr, uint8_t* next_id) {
  if (max_sensors < 2) return 0;  // Need space for 2 sensors

  // Temperature sensor
  sensors[0].sensor_id = (*next_id)++;
  sensors[0].i2c_addr = addr;
  sensors[0].sensor_type = SENSOR_TYPE_TEMPERATURE;
  strncpy(sensors[0].label, "SHT40 Temp", 16);
  sensors[0].active = true;

  // Humidity sensor
  sensors[1].sensor_id = (*next_id)++;
  sensors[1].i2c_addr = addr;
  sensors[1].sensor_type = SENSOR_TYPE_HUMIDITY;
  strncpy(sensors[1].label, "SHT40 Humidity", 16);
  sensors[1].active = true;

  return 2;  // Added 2 sensors
}
