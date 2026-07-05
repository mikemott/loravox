#pragma once
#include <Wire.h>
#include <Adafruit_SHT4x.h>
#include "../SensorRegistry.h"

// SHT40 Temperature & Humidity Sensor
// I2C Addresses: 0x44 (default), 0x45 (alternate)

extern Adafruit_SHT4x sht4;

bool detectSHT40(TwoWire* wire, uint8_t addr);
uint8_t addSHT40Sensors(I2CSensor* sensors, uint8_t max_sensors, uint8_t addr, uint8_t* next_id);
