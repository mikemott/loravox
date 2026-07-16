#pragma once
#include <Wire.h>
#include <Adafruit_BMP5xx.h>
#include "../SensorRegistry.h"

// BMP581 Pressure & Temperature Sensor
// I2C Addresses: 0x46 (SDO to GND), 0x47 (SDO to VDD)

extern Adafruit_BMP5xx bmp5;

bool detectBMP581(TwoWire* wire, uint8_t addr);
uint8_t addBMP581Sensors(I2CSensor* sensors, uint8_t max_sensors, uint8_t addr, uint8_t* next_id);
