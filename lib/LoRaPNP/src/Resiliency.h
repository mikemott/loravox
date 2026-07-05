#pragma once
#include <Arduino.h>
#include "Types.h"

namespace lorapnp {

struct HealthMetrics {
    uint32_t total_packets_rx;
    uint32_t total_packets_tx;
    uint32_t crc_errors;
    uint32_t timeout_errors;
    uint32_t radio_resets;
    uint32_t uptime_seconds;
    uint32_t free_heap_min;
    uint32_t free_heap_current;
    uint8_t active_nodes;
    uint8_t stale_nodes;
    uint32_t consecutive_failures;
    uint32_t rediscoveries;

    HealthMetrics()
        : total_packets_rx(0), total_packets_tx(0)
        , crc_errors(0), timeout_errors(0), radio_resets(0)
        , uptime_seconds(0), free_heap_min(UINT32_MAX), free_heap_current(0)
        , active_nodes(0), stale_nodes(0)
        , consecutive_failures(0), rediscoveries(0) {}
};

struct LinkQuality {
    int16_t rssi;
    float snr;
    uint8_t quality_score;

    LinkQuality() : rssi(0), snr(0.0f), quality_score(0) {}
};

inline uint8_t calculateQualityScore(int16_t rssi, float snr) {
    if (rssi > -60 && snr > 8) return 90;
    if (rssi > -80 && snr > 5) return 70;
    if (rssi > -100 && snr > 2) return 50;
    return 30;
}

inline uint8_t getAdaptiveMaxRetries(uint8_t quality_score) {
    if (quality_score >= 80) return 2;
    if (quality_score >= 60) return 3;
    if (quality_score >= 40) return 5;
    return 7;
}

inline uint32_t getAdaptiveTimeout(uint8_t quality_score) {
    if (quality_score >= 80) return 4000;
    if (quality_score >= 60) return 6000;
    if (quality_score >= 40) return 8000;
    return 12000;
}

// Mirror the protocol-level stale timeout used by the registry.
constexpr uint32_t NODE_STALE_TIMEOUT_MS = STATION_STALE_TIMEOUT_MS;
constexpr uint32_t HEAP_WARNING_THRESHOLD = 10240;
constexpr uint32_t CONSECUTIVE_FAILURE_THRESHOLD = 10;
constexpr uint32_t HEAP_CHECK_INTERVAL_MS = 60000;

} // namespace lorapnp
