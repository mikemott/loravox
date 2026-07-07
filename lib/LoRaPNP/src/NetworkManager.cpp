#include "NetworkManager.h"
#include <time.h>

namespace lorapnp {

NetworkManager::NetworkManager()
    : current_transport(TRANSPORT_LORA)
    , transport_established_ms(0)
    , last_connectivity_check_ms(0)
    , transport_switch_count(0)
    , battery_voltage(0.0f) {

    // Default config
    config.daily_check_enabled = true;
    config.daily_check_hour = 3;  // 3 AM
    config.min_battery_for_check = 3.5f;
    config.battery_aware_mode = true;
}

NetworkManager::~NetworkManager() {
    end();
}

bool NetworkManager::begin() {
    // Load WiFi credentials from NVS if available
    if (prefs.begin("network", true)) {  // Read-only
        String ssid = prefs.getString("wifi_ssid", "");
        String password = prefs.getString("wifi_password", "");
        bool wifi_enabled = prefs.getBool("wifi_enabled", false);
        prefs.end();

        if (ssid.length() > 0) {
            wifi_transport.setCredentials(ssid.c_str(), password.c_str());
            wifi_transport.enable(wifi_enabled);
        }
    }

    // Initialize WiFi transport
    if (!wifi_transport.begin()) {
        ESP_LOGW("Network", "WiFi transport not available");
    }

    // Start with LoRa as default (always available)
    current_transport = TRANSPORT_LORA;
    transport_established_ms = millis();

    // Trigger initial transport selection
    selectPrimaryTransport();

    return true;
}

void NetworkManager::end() {
    wifi_transport.end();
}

void NetworkManager::update() {
    // Update all transports (handles async state machines)
    wifi_transport.update();

    // Check if WiFi connection state changed and update transport selection
    static bool last_wifi_connected = false;
    bool wifi_connected = wifi_transport.isConnected();

    if (wifi_connected != last_wifi_connected) {
        ESP_LOGI("Network", "WiFi connection state changed: %s",
                 wifi_connected ? "CONNECTED" : "DISCONNECTED");
        selectPrimaryTransport();  // Re-evaluate transport priority
        last_wifi_connected = wifi_connected;
    }

    // Check for daily connectivity check
    if (config.daily_check_enabled) {
        performDailyCheck();
    }
}

void NetworkManager::setWiFiCredentials(const char* ssid, const char* password) {
    wifi_transport.setCredentials(ssid, password);

    // Save to NVS
    if (prefs.begin("network", false)) {  // Read-write
        prefs.putString("wifi_ssid", ssid);
        prefs.putString("wifi_password", password);
        prefs.end();
    }
}

void NetworkManager::enableWiFi(bool enabled) {
    wifi_transport.enable(enabled);

    // Save to NVS
    if (prefs.begin("network", false)) {
        prefs.putBool("wifi_enabled", enabled);
        prefs.end();
    }

    if (enabled) {
        // Trigger transport selection
        selectPrimaryTransport();
    }
}

void NetworkManager::setConfig(const NetworkConfig& _config) {
    config = _config;
}

TransportType NetworkManager::selectPrimaryTransport() {
    TransportType best_transport = TRANSPORT_LORA;  // Default fallback

    // Battery-aware mode: prefer BLE over WiFi if battery is low
    bool low_battery = (battery_voltage > 0 && battery_voltage < 3.3f);

    if (config.battery_aware_mode && low_battery) {
        ESP_LOGI("Network", "Low battery (%.2fV), preferring BLE/LoRa over WiFi",
                 battery_voltage);
        // Skip WiFi, go straight to BLE (when implemented)
        best_transport = TRANSPORT_LORA;
    } else {
        // Try WiFi first (highest priority)
        if (wifi_transport.isAvailable()) {
            if (wifi_transport.isConnected()) {
                best_transport = TRANSPORT_WIFI;
            } else {
                // WiFi is enabled but not connected
                // Async state machine will handle connection
                wifi_transport.startConnection();
            }
        }

        // TODO: Try BLE next (when implemented)
        // if (ble_transport.isAvailable() && ble_transport.isConnected()) {
        //     best_transport = TRANSPORT_BLE;
        // }
    }

    // Switch if different from current
    if (best_transport != current_transport) {
        switchTransport(best_transport);
    }

    return current_transport;
}

ITransport* NetworkManager::getActiveTransport() {
    switch (current_transport) {
        case TRANSPORT_WIFI:
            return &wifi_transport;
        // case TRANSPORT_BLE:
        //     return &ble_transport;
        case TRANSPORT_LORA:
        default:
            return nullptr;  // LoRa is handled separately for now
    }
}

void NetworkManager::performDailyCheck() {
    if (!shouldPerformDailyCheck()) {
        return;
    }

    // Skip if battery is low
    if (battery_voltage > 0 && battery_voltage < config.min_battery_for_check) {
        ESP_LOGI("Network", "Skipping daily check - battery %.2fV < %.2fV",
                 battery_voltage, config.min_battery_for_check);
        return;
    }

    // Skip if already on WiFi
    if (current_transport == TRANSPORT_WIFI) {
        ESP_LOGD("Network", "Already on WiFi, skipping daily check");
        return;
    }

    ESP_LOGI("Network", "Performing daily connectivity check");

    // Try to establish WiFi without disrupting current connection
    if (wifi_transport.isAvailable() && !wifi_transport.isConnected()) {
        wifi_transport.startConnection();

        // If LoRa is current transport and WiFi succeeds, switch
        // The update() loop will handle the actual connection
    }

    last_connectivity_check_ms = millis();
}

TransportMetrics NetworkManager::getTransportMetrics(TransportType type) {
    switch (type) {
        case TRANSPORT_WIFI:
            return wifi_transport.getMetrics();
        // case TRANSPORT_BLE:
        //     return ble_transport.getMetrics();
        default: {
            TransportMetrics empty = {0};
            return empty;
        }
    }
}

void NetworkManager::setBatteryVoltage(float voltage) {
    battery_voltage = voltage;
}

void NetworkManager::switchTransport(TransportType new_transport) {
    if (new_transport == current_transport) {
        return;
    }

    ESP_LOGI("Network", "Switching transport: %d -> %d",
             current_transport, new_transport);

    current_transport = new_transport;
    transport_established_ms = millis();
    transport_switch_count++;
}

bool NetworkManager::shouldPerformDailyCheck() {
    // Only check once per day at configured hour
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return false;  // No time available
    }

    if (timeinfo.tm_hour != config.daily_check_hour) {
        return false;  // Not the right hour
    }

    // Check if we've already checked today
    uint32_t now = millis();
    if (now - last_connectivity_check_ms < 86400000) {  // 24 hours
        return false;
    }

    return true;
}

} // namespace lorapnp
