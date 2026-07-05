#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "Types.h"

using namespace lorapnp;

// I2C sensor structure
struct I2CSensor {
  uint8_t sensor_id;
  uint8_t i2c_addr;
  uint8_t sensor_type;
  char label[17];
  bool active;
};

// Sensor driver interface
struct SensorDriver {
  const char* name;
  uint8_t addresses[4];  // Up to 3 addresses + 0 terminator
  bool (*detect)(TwoWire* wire, uint8_t addr);
  uint8_t (*addSensors)(I2CSensor* sensors, uint8_t max_sensors, uint8_t addr, uint8_t* next_id);
};

// Detect all I2C sensors on the bus
// Returns number of sensors detected
uint8_t detectAllI2CSensors(TwoWire* wire, I2CSensor* sensors, uint8_t max_sensors, uint8_t start_id);

// Scan I2C bus and print devices found
void scanI2CBus(TwoWire* wire);
