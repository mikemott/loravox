#ifndef WIFI_TRANSPORT_H
#define WIFI_TRANSPORT_H

#include "Transport.h"
#include "Packet.h"
#include <WiFi.h>
#include <queue>
#include <deque>

namespace lorapnp {

enum WiFiState {
    WIFI_STATE_DISABLED,
    WIFI_STATE_IDLE,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_FAILED
};

struct WiFiRetryState {
    WiFiState state;
    uint32_t attempt_start_ms;
    uint32_t next_retry_ms;
    uint32_t retry_window_start_ms;
    uint8_t attempt_count;
    uint32_t retry_delay_ms;
};

class WiFiTransport : public ITransport {
public:
    WiFiTransport();
    ~WiFiTransport();

    // Configuration
    bool setCredentials(const char* ssid, const char* password);
    void enable(bool enabled);

    // ITransport interface
    bool begin() override;
    void end() override;
    void update() override;

    bool isConnected() override;
    bool isAvailable() override;

    bool send(const Packet* packet) override;
    bool receive(Packet* packet) override;

    TransportMetrics getMetrics() override;
    TransportType getType() override { return TRANSPORT_WIFI; }
    uint8_t getPriority() override { return 100; }  // Highest priority

    // Connection management
    void startConnection();
    void disconnect();

    WiFiState getState() { return retry_state.state; }
    int16_t getRSSI() { return WiFi.RSSI(); }

private:
    void updateConnectionState();
    void handleConnecting();
    void handleConnected();
    void handleFailed();
    void scheduleRetry(uint32_t delay_ms);

    char ssid[33];
    char password[65];
    bool enabled;

    WiFiRetryState retry_state;
    TransportMetrics metrics;

    // Packet queue (WiFi sends immediately, but we track for metrics)
    std::queue<Packet, std::deque<Packet>> rx_queue;

    static constexpr uint32_t RETRY_WINDOW_MS = 300000;  // 5 minutes
    static constexpr uint32_t ATTEMPT_TIMEOUT_MS = 10000;  // 10 seconds per attempt
    static constexpr uint32_t INITIAL_RETRY_DELAY_MS = 10000;  // 10 seconds
    static constexpr uint32_t MAX_RETRY_DELAY_MS = 120000;  // 2 minutes
    static constexpr int8_t MIN_RSSI = -75;  // Below this, quality is poor
};

} // namespace lorapnp

#endif // WIFI_TRANSPORT_H
