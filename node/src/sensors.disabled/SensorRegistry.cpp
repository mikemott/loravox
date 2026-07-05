#include "SensorRegistry.h"
#include "drivers/SHT40Driver.h"
#include "drivers/BMP581Driver.h"
#include "drivers/SoilMoistureDriver.h"
#include "drivers/XM125Driver.h"

// Registry of all known I2C sensors
const SensorDriver i2c_sensor_drivers[] = {
  {
    .name = "SHT40",
    .addresses = {0x44, 0x45, 0},
    .detect = detectSHT40,
    .addSensors = addSHT40Sensors
  },
  {
    .name = "BMP581",
    .addresses = {0x46, 0x47, 0},
    .detect = detectBMP581,
    .addSensors = addBMP581Sensors
  },
  {
    .name = "Soil Moisture",
    .addresses = {0x28, 0},
    .detect = detectSoilMoisture,
    .addSensors = addSoilMoistureSensor
  },
  {
    .name = "XM125",
    .addresses = {0x52, 0},
    .detect = detectXM125,
    .addSensors = addXM125Sensor
  }
};

const uint8_t num_drivers = sizeof(i2c_sensor_drivers) / sizeof(SensorDriver);

void scanI2CBus(TwoWire* wire) {
  Serial.println("======================================");
  Serial.println("Scanning I2C bus...");

  uint8_t device_count = 0;
  for (uint8_t addr = 0x03; addr < 0x78; addr++) {
    wire->beginTransmission(addr);
    uint8_t error = wire->endTransmission();

    if (error == 0) {
      Serial.printf("  0x%02X", addr);

      // Try to identify the device
      bool identified = false;
      for (uint8_t i = 0; i < num_drivers; i++) {
        for (uint8_t j = 0; j < 4 && i2c_sensor_drivers[i].addresses[j] != 0; j++) {
          if (i2c_sensor_drivers[i].addresses[j] == addr) {
            Serial.printf(" (possible %s)", i2c_sensor_drivers[i].name);
            identified = true;
            break;
          }
        }
        if (identified) break;
      }

      Serial.println();
      device_count++;
    }
  }

  if (device_count == 0) {
    Serial.println("No I2C devices found!");
  } else {
    Serial.printf("Found %u I2C device(s)\n", device_count);
  }
  Serial.println("======================================\n");
}

uint8_t detectAllI2CSensors(TwoWire* wire, I2CSensor* sensors, uint8_t max_sensors, uint8_t start_id) {
  uint8_t sensor_count = 0;
  uint8_t next_sensor_id = start_id;

  Serial.println("Detecting I2C sensors...");

  // Try each driver
  for (uint8_t i = 0; i < num_drivers && sensor_count < max_sensors; i++) {
    const SensorDriver& driver = i2c_sensor_drivers[i];

    // Try each possible address for this driver
    for (uint8_t j = 0; j < 4 && driver.addresses[j] != 0; j++) {
      uint8_t addr = driver.addresses[j];

      // Check if address responds
      wire->beginTransmission(addr);
      if (wire->endTransmission() != 0) {
        continue;  // Address doesn't respond
      }

      // Try driver-specific detection
      if (driver.detect(wire, addr)) {
        Serial.printf("✓ Found %s at 0x%02X\n", driver.name, addr);

        // Let driver add its sensors
        uint8_t added = driver.addSensors(&sensors[sensor_count],
                                          max_sensors - sensor_count,
                                          addr,
                                          &next_sensor_id);
        sensor_count += added;

        break;  // Found this sensor, don't check other addresses
      }
    }
  }

  Serial.printf("Detected %u I2C sensor(s)\n\n", sensor_count);
  return sensor_count;
}
