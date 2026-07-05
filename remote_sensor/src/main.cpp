#include <Arduino.h>
#include <Wire.h>
#include <SSD1306Wire.h>
#include <Adafruit_SHT4x.h>
#include <NimBLEDevice.h>
#include "BLEProtocol.h"
#include "Types.h"

using namespace lorapnp;

// Pin definitions for T3-S3
#define LED_PIN   37
#define OLED_SDA  18
#define OLED_SCL  17
#define QWIIC_SDA 43
#define QWIIC_SCL 44
#define BATTERY_ADC_PIN 1  // GPIO1 for battery voltage measurement

// Hardware instances
SSD1306Wire display(0x3c, OLED_SDA, OLED_SCL);
Adafruit_SHT4x sht4;

// Device identity
uint8_t device_uid[6];
char device_uid_str[18];

// Configuration
uint32_t sleep_interval_ms = BLE_ADV_INTERVAL_MS;
bool display_enabled = true;
bool first_boot = true;

// Sensor readings
float last_temperature = 0.0f;
float last_humidity = 0.0f;
uint16_t battery_mv = 0;

// Boot count (persisted in RTC memory across deep sleep)
RTC_DATA_ATTR uint32_t boot_count = 0;

/**
 * Initialize device UID from MAC address
 */
void loadDeviceUID() {
    uint64_t mac = ESP.getEfuseMac();
    for (uint8_t i = 0; i < 6; i++) {
        device_uid[i] = (mac >> (8 * (5 - i))) & 0xFF;
    }
    formatBLEUID(device_uid, device_uid_str, sizeof(device_uid_str));
}

/**
 * Update OLED display with sensor readings
 */
void updateDisplay(const char* line1, const char* line2 = "", const char* line3 = "") {
    if (!display_enabled) return;

    display.clear();
    display.drawString(0, 0, line1);
    display.drawString(0, 20, line2);
    display.drawString(0, 40, line3);
    display.display();
}

/**
 * Read battery voltage via ADC
 * T3-S3 has voltage divider on battery pin (2:1 ratio)
 * ADC range: 0-3.3V, 12-bit (0-4095)
 */
uint16_t readBatteryVoltage() {
    // Read ADC multiple times and average
    uint32_t sum = 0;
    const uint8_t samples = 8;

    for (uint8_t i = 0; i < samples; i++) {
        sum += analogRead(BATTERY_ADC_PIN);
        delay(10);
    }

    uint16_t adc_value = sum / samples;

    // Convert ADC to voltage (assuming 2:1 divider, 3.3V reference)
    // voltage_mv = (adc_value / 4095.0) * 3300 * 2
    float voltage = (adc_value / 4095.0f) * 3300.0f * 2.0f;

    // For testing with USB power, clamp to reasonable range
    if (voltage > 5000) voltage = 3700;  // USB power

    return (uint16_t)voltage;
}

/**
 * Read SHT40 sensor
 */
bool readSensor() {
    sensors_event_t humidity, temp;

    if (!sht4.getEvent(&humidity, &temp)) {
        Serial.println("Failed to read SHT40 sensor");
        return false;
    }

    last_temperature = temp.temperature;
    last_humidity = humidity.relative_humidity;

    Serial.printf("Sensor: %.2f°C, %.1f%% RH\n",
                  last_temperature, last_humidity);

    return true;
}

/**
 * Advertise sensor data via BLE
 */
void advertiseSensorData() {
    // Build advertisement packet
    BLESensorAdvertisement adv;
    initBLEAdvertisement(adv, device_uid);

    // Add temperature reading
    addBLEReading(adv, 1, SENSOR_TYPE_TEMPERATURE, SENSOR_UNIT_CELSIUS,
                  SENSOR_DATA_FLOAT32, last_temperature);

    // Add humidity reading
    addBLEReading(adv, 2, SENSOR_TYPE_HUMIDITY, SENSOR_UNIT_PERCENT,
                  SENSOR_DATA_FLOAT32, last_humidity);

    // Set battery and flags
    adv.battery_mv = battery_mv;
    adv.flags = BLE_FLAG_NONE;

    if (first_boot) {
        adv.flags |= BLE_FLAG_FIRST_BOOT;
        first_boot = false;
    }

    if (isBatteryLow(battery_mv)) {
        adv.flags |= BLE_FLAG_LOW_BATTERY;
    }

    // Initialize BLE
    NimBLEDevice::init("LoRaPNP");
    NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();

    // Set advertising data
    NimBLEAdvertisementData advData;
    std::string mfg_data((char*)&adv, sizeof(adv));
    advData.setManufacturerData(mfg_data);
    advData.setCompleteServices(NimBLEUUID(BLE_SERVICE_UUID));

    advertising->setAdvertisementData(advData);
    advertising->setAdvertisementType(BLE_GAP_CONN_MODE_NON);  // Non-connectable

    // Start advertising
    Serial.printf("BLE advertising: %.2f°C, %.1f%% RH, %u mV\n",
                  last_temperature, last_humidity, battery_mv);

    digitalWrite(LED_PIN, HIGH);
    advertising->start();
    delay(BLE_ADV_DURATION_MS);
    advertising->stop();
    digitalWrite(LED_PIN, LOW);

    // Deinit BLE to save power
    NimBLEDevice::deinit(true);

    Serial.println("BLE advertising complete");
}

/**
 * Enter deep sleep
 */
void enterDeepSleep() {
    Serial.printf("Entering deep sleep for %lu seconds (boot #%lu)\n",
                  sleep_interval_ms / 1000, boot_count);

    if (display_enabled) {
        char line2[32], line3[32];
        snprintf(line2, sizeof(line2), "Sleeping %lus", sleep_interval_ms / 1000);
        snprintf(line3, sizeof(line3), "Boot #%lu", boot_count);
        updateDisplay("Remote Sensor", line2, line3);
        delay(500);
    }

    // Disable display
    display.displayOff();

    // Configure wake timer
    esp_sleep_enable_timer_wakeup(sleep_interval_ms * 1000ULL);

    // Enter deep sleep
    esp_deep_sleep_start();
}

void setup() {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    // Increment boot count
    boot_count++;

    // Initialize serial
    Serial.begin(115200);
    delay(500);

    // Check wake reason
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

    Serial.println("\n=== LoRa PNP Remote Sensor ===");
    if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
        Serial.printf("Wake from sleep (boot #%lu)\n", boot_count);
    } else {
        Serial.println("Cold boot");
        first_boot = true;
    }

    // Initialize device UID
    loadDeviceUID();
    Serial.printf("Device UID: %s\n", device_uid_str);

    // Initialize display
    Wire.begin(OLED_SDA, OLED_SCL);
    display.init();
    display.flipScreenVertically();
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_LEFT);

    if (display_enabled) {
        updateDisplay("Remote Sensor", device_uid_str, "Initializing...");
    }

    // Initialize I2C for sensors
    Wire1.begin(QWIIC_SDA, QWIIC_SCL);

    // Initialize SHT40
    if (!sht4.begin(&Wire1)) {
        Serial.println("ERROR: SHT40 not found!");
        Serial.println("Check: 1) Sensor connected? 2) QWIIC cable? 3) Power?");

        if (display_enabled) {
            updateDisplay("Remote Sensor", "ERROR", "SHT40 not found!");
        }

        // Blink LED error pattern
        for (int i = 0; i < 5; i++) {
            digitalWrite(LED_PIN, HIGH);
            delay(200);
            digitalWrite(LED_PIN, LOW);
            delay(200);
        }

        // Sleep and try again
        delay(2000);
        enterDeepSleep();
        return;
    }

    Serial.println("SHT40 initialized");
    sht4.setPrecision(SHT4X_HIGH_PRECISION);
    sht4.setHeater(SHT4X_NO_HEATER);

    // Read battery voltage
    analogReadResolution(12);  // 12-bit ADC
    battery_mv = readBatteryVoltage();
    Serial.printf("Battery: %u mV\n", battery_mv);

    if (isBatteryLow(battery_mv)) {
        Serial.println("WARNING: Battery low!");
    }

    // Flash LED to indicate ready
    for (int i = 0; i < 3; i++) {
        digitalWrite(LED_PIN, HIGH);
        delay(100);
        digitalWrite(LED_PIN, LOW);
        delay(100);
    }
}

void loop() {
    // Check for serial commands
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();

        if (cmd == "status") {
            Serial.printf("Device UID: %s\n", device_uid_str);
            Serial.printf("Temperature: %.2f°C\n", last_temperature);
            Serial.printf("Humidity: %.1f%% RH\n", last_humidity);
            Serial.printf("Battery: %u mV\n", battery_mv);
            Serial.printf("Boot count: %lu\n", boot_count);
            Serial.printf("Sleep interval: %lu ms\n", sleep_interval_ms);
            Serial.printf("Display: %s\n", display_enabled ? "enabled" : "disabled");

        } else if (cmd == "sleep on") {
            Serial.println("Deep sleep enabled - will sleep after next cycle");

        } else if (cmd == "sleep off") {
            Serial.println("Deep sleep disabled - staying awake");
            sleep_interval_ms = 0;

        } else if (cmd == "display on") {
            display_enabled = true;
            display.displayOn();
            Serial.println("Display enabled");

        } else if (cmd == "display off") {
            display_enabled = false;
            display.displayOff();
            Serial.println("Display disabled (power saving)");

        } else if (cmd == "factory" || cmd == "reset") {
            Serial.println("Resetting boot count and restarting...");
            boot_count = 0;
            delay(1000);
            ESP.restart();

        } else if (cmd == "read") {
            battery_mv = readBatteryVoltage();
            if (readSensor()) {
                Serial.printf("%.2f°C, %.1f%% RH, %u mV\n",
                              last_temperature, last_humidity, battery_mv);
            }

        } else if (cmd == "advertise") {
            Serial.println("Manual BLE advertisement triggered");
            battery_mv = readBatteryVoltage();
            if (readSensor()) {
                advertiseSensorData();
            }

        } else if (cmd.length() > 0) {
            Serial.println("Commands: status, read, advertise, display on/off, sleep on/off, factory, reset");
        }

        return;  // Don't sleep if processing commands
    }

    // Main sensor cycle
    Serial.println("\n--- Sensor Cycle Start ---");

    // Read battery
    battery_mv = readBatteryVoltage();

    // Read sensor
    if (!readSensor()) {
        Serial.println("Sensor read failed, skipping advertisement");
        delay(5000);
        return;
    }

    // Update display
    if (display_enabled) {
        char line2[32], line3[32];
        snprintf(line2, sizeof(line2), "%.1fC %.0f%% %umV",
                 last_temperature, last_humidity, battery_mv);
        snprintf(line3, sizeof(line3), "Boot #%lu", boot_count);
        updateDisplay("Remote Sensor", line2, line3);
    }

    // Advertise via BLE
    advertiseSensorData();

    // Sleep or loop
    if (sleep_interval_ms > 0) {
        Serial.println("--- Cycle Complete ---\n");
        delay(100);  // Let serial flush
        enterDeepSleep();
    } else {
        // Stay awake for testing
        Serial.println("--- Cycle Complete (staying awake) ---\n");
        delay(sleep_interval_ms > 0 ? sleep_interval_ms : 10000);
    }
}
