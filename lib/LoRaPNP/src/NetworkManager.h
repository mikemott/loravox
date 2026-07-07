#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include "Transport.h"
#include "WiFiTransport.h"
#include <Preferences.h>

namespace lorapnp {

struct NetworkConfig {
    bool daily_check_enabled;
    uint8_t daily_check_hour;  // 0-23
    float min_battery_for_check;  // Voltage threshold
    bool battery_aware_mode;  // Prefer BLE over WiFi on low battery
};

class NetworkManager {
public:
    NetworkManager();
    ~NetworkManager();

    // Initialization
    bool begin();
    void end();

    // Must be called from main loop
    void update();

    // Configuration
    void setWiFiCredentials(const char* ssid, const char* password);
    void enableWiFi(bool enabled);
    void setConfig(const NetworkConfig& config);

    // Transport selection
    TransportType selectPrimaryTransport();
    TransportType getCurrentTransport() { return current_transport; }
    ITransport* getActiveTransport();

    // Daily connectivity check
    void performDailyCheck();

    // Metrics
    TransportMetrics getTransportMetrics(TransportType type);
    uint32_t getTransportSwitchCount() { return transport_switch_count; }

    // Battery awareness
    void setBatteryVoltage(float voltage);

private:
    void switchTransport(TransportType new_transport);
    bool shouldPerformDailyCheck();

    WiFiTransport wifi_transport;
    // BLE and LoRa transports added later

    TransportType current_transport;
    uint32_t transport_established_ms;
    uint32_t last_connectivity_check_ms;
    uint32_t transport_switch_count;

    float battery_voltage;
    NetworkConfig config;

    Preferences prefs;
};

} // namespace lorapnp

#endif // NETWORK_MANAGER_H
