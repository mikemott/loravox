#include "BMP581Driver.h"

Adafruit_BMP5xx bmp5;

bool detectBMP581(TwoWire* wire, uint8_t addr) {
  return bmp5.begin(addr, wire);
}

uint8_t addBMP581Sensors(I2CSensor* sensors, uint8_t max_sensors, uint8_t addr, uint8_t* next_id) {
  if (max_sensors < 1) return 0;

  // Pressure sensor (BMP581 also has temp, but SHT40 is more accurate)
  sensors[0].sensor_id = (*next_id)++;
  sensors[0].i2c_addr = addr;
  sensors[0].sensor_type = SENSOR_TYPE_PRESSURE;
  strncpy(sensors[0].label, "BMP581 Pressure", 16);
  sensors[0].active = true;

  return 1;  // Added 1 sensor
}
