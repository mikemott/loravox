#include "WiFiTransport.h"
#include <esp_wifi.h>

namespace lorapnp {

WiFiTransport::WiFiTransport() : enabled(false) {
    memset(&retry_state, 0, sizeof(retry_state));
    memset(&metrics, 0, sizeof(metrics));
    retry_state.state = WIFI_STATE_DISABLED;
    retry_state.retry_delay_ms = INITIAL_RETRY_DELAY_MS;
}

WiFiTransport::~WiFiTransport() {
    end();
}

bool WiFiTransport::setCredentials(const char* _ssid, const char* _password) {
    if (!_ssid || strlen(_ssid) == 0) {
        return false;
    }

    strncpy(ssid, _ssid, sizeof(ssid) - 1);
    ssid[sizeof(ssid) - 1] = '\0';

    if (_password) {
        strncpy(password, _password, sizeof(password) - 1);
        password[sizeof(password) - 1] = '\0';
    } else {
        password[0] = '\0';
    }

    return true;
}

void WiFiTransport::enable(bool _enabled) {
    if (_enabled && !enabled && strlen(ssid) > 0) {
        enabled = true;
        retry_state.state = WIFI_STATE_IDLE;
        startConnection();
    } else if (!_enabled && enabled) {
        enabled = false;
        disconnect();
    }
}

bool WiFiTransport::begin() {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);  // We handle reconnection manually

    if (enabled && strlen(ssid) > 0) {
        retry_state.state = WIFI_STATE_IDLE;
        return true;
    }

    retry_state.state = WIFI_STATE_DISABLED;
    return false;
}

void WiFiTransport::end() {
    disconnect();
    retry_state.state = WIFI_STATE_DISABLED;
}

void WiFiTransport::update() {
    if (!enabled) {
        return;
    }

    updateConnectionState();
}

bool WiFiTransport::isConnected() {
    return retry_state.state == WIFI_STATE_CONNECTED &&
           WiFi.status() == WL_CONNECTED;
}

bool WiFiTransport::isAvailable() {
    return enabled && strlen(ssid) > 0;
}

bool WiFiTransport::send(const Packet* packet) {
    if (!isConnected()) {
        metrics.failures++;
        return false;
    }

    // WiFi transport uses UDP/TCP at application layer
    // This is a placeholder - actual implementation depends on protocol
    // For now, just track metrics
    metrics.packets_sent++;
    metrics.bytes_sent += packet->payload_len + 12;  // 12-byte header

    return true;
}

bool WiFiTransport::receive(Packet* packet) {
    if (!isConnected() || rx_queue.empty()) {
        return false;
    }

    *packet = rx_queue.front();
    rx_queue.pop();

    metrics.packets_received++;
    metrics.bytes_received += packet->payload_len + 12;

    return true;
}

TransportMetrics WiFiTransport::getMetrics() {
    metrics.rssi = WiFi.RSSI();
    metrics.snr = 0.0f;  // Not applicable for WiFi
    return metrics;
}

void WiFiTransport::startConnection() {
    if (!enabled || strlen(ssid) == 0) {
        return;
    }

    retry_state.state = WIFI_STATE_IDLE;
    retry_state.retry_window_start_ms = millis();
    retry_state.next_retry_ms = millis();  // Start immediately
    retry_state.attempt_count = 0;
    retry_state.retry_delay_ms = INITIAL_RETRY_DELAY_MS;
}

void WiFiTransport::disconnect() {
    WiFi.disconnect(true);
    retry_state.state = enabled ? WIFI_STATE_IDLE : WIFI_STATE_DISABLED;

    // Clear queues
    while (!rx_queue.empty()) {
        rx_queue.pop();
    }
}

void WiFiTransport::updateConnectionState() {
    uint32_t now = millis();

    switch (retry_state.state) {
        case WIFI_STATE_DISABLED:
            // Do nothing
            break;

        case WIFI_STATE_IDLE:
            // Check if it's time to retry
            if (now >= retry_state.next_retry_ms) {
                WiFi.begin(ssid, password);
                retry_state.state = WIFI_STATE_CONNECTING;
                retry_state.attempt_start_ms = now;
                retry_state.attempt_count++;
                ESP_LOGI("WiFi", "Connection attempt %u", retry_state.attempt_count);
            }
            break;

        case WIFI_STATE_CONNECTING:
            handleConnecting();
            break;

        case WIFI_STATE_CONNECTED:
            handleConnected();
            break;

        case WIFI_STATE_FAILED:
            handleFailed();
            break;
    }
}

void WiFiTransport::handleConnecting() {
    uint32_t now = millis();

    if (WiFi.status() == WL_CONNECTED) {
        retry_state.state = WIFI_STATE_CONNECTED;
        ESP_LOGI("WiFi", "Connected after %u attempts (RSSI: %d dBm)",
                 retry_state.attempt_count, WiFi.RSSI());
        return;
    }

    // Check timeout
    if (now - retry_state.attempt_start_ms > ATTEMPT_TIMEOUT_MS) {
        WiFi.disconnect();

        // Check if we've exceeded retry window
        if (now - retry_state.retry_window_start_ms > RETRY_WINDOW_MS) {
            retry_state.state = WIFI_STATE_FAILED;
            ESP_LOGW("WiFi", "Failed after 5 minutes, giving up");
            metrics.failures++;
            return;
        }

        // Schedule next retry with exponential backoff
        retry_state.retry_delay_ms = min(MAX_RETRY_DELAY_MS,
                                         retry_state.retry_delay_ms * 2);
        scheduleRetry(retry_state.retry_delay_ms);

        ESP_LOGD("WiFi", "Attempt %u failed, retry in %u ms",
                 retry_state.attempt_count, retry_state.retry_delay_ms);
    }
}

void WiFiTransport::handleConnected() {
    // Monitor for disconnection
    if (WiFi.status() != WL_CONNECTED) {
        ESP_LOGW("WiFi", "Connection lost");
        retry_state.state = WIFI_STATE_IDLE;
        retry_state.retry_window_start_ms = millis();
        scheduleRetry(30000);  // Retry in 30 seconds
        metrics.failures++;
    }
}

void WiFiTransport::handleFailed() {
    // Stay in failed state - can be reset by calling startConnection()
}

void WiFiTransport::scheduleRetry(uint32_t delay_ms) {
    retry_state.next_retry_ms = millis() + delay_ms;
    retry_state.state = WIFI_STATE_IDLE;
}

} // namespace lorapnp
