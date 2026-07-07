#ifndef TRANSPORT_H
#define TRANSPORT_H

#include <Arduino.h>
#include "Packet.h"

namespace lorapnp {

enum TransportType {
    TRANSPORT_LORA = 0,
    TRANSPORT_BLE = 1,
    TRANSPORT_WIFI = 2,
    TRANSPORT_NONE = 255
};

struct TransportMetrics {
    uint32_t packets_sent;
    uint32_t bytes_sent;
    uint32_t packets_received;
    uint32_t bytes_received;
    uint32_t failures;
    int16_t rssi;  // Signal strength
    float snr;     // Signal-to-noise ratio (LoRa only)
};

class ITransport {
public:
    virtual ~ITransport() {}

    // Lifecycle
    virtual bool begin() = 0;
    virtual void end() = 0;
    virtual void update() = 0;  // Called from main loop

    // Connection state
    virtual bool isConnected() = 0;
    virtual bool isAvailable() = 0;  // Can this transport be used?

    // Data transfer
    virtual bool send(const Packet* packet) = 0;
    virtual bool receive(Packet* packet) = 0;

    // Metrics
    virtual TransportMetrics getMetrics() = 0;
    virtual TransportType getType() = 0;

    // Priority (higher = preferred)
    virtual uint8_t getPriority() = 0;
};

} // namespace lorapnp

#endif // TRANSPORT_H
