#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>
#include "Types.h"
#include "Packet.h"

namespace lorapnp {

struct RadioConfig {
    float frequency;
    uint8_t sf;
    float bandwidth;
    uint8_t sync_word;
    int8_t tx_power;
};

class Radio {
public:
    Radio(uint8_t cs, uint8_t dio0, uint8_t rst, uint8_t dio1,
          uint8_t sck, uint8_t miso, uint8_t mosi);

    Status init(const RadioConfig& config);
    Status sendPacket(const Packet& pkt);
    Status receivePacket(Packet& pkt, uint32_t timeout_ms = RECEIVE_TIMEOUT_MS);

    int16_t getRSSI() const { return rssi_; }
    float getSNR() const { return snr_; }

    bool needsReset() const;
    Status reset();
    void recordOperationFailure();
    void recordOperationSuccess();
    uint32_t getConsecutiveFailures() const;

private:
    uint8_t cs_, dio0_, rst_, dio1_;
    uint8_t sck_, miso_, mosi_;
    Module* module_;
    SX1276* radio_;
    uint8_t tx_buf_[HEADER_SIZE + MAX_PAYLOAD_SIZE + 2];
    uint8_t rx_buf_[HEADER_SIZE + MAX_PAYLOAD_SIZE + 2];
    int16_t rssi_;
    float snr_;
    RadioConfig config_;
    uint32_t consecutive_failures_;
    uint32_t last_successful_op_ms_;
    static constexpr uint32_t RADIO_FAILURE_THRESHOLD = 5;
    static constexpr uint32_t RADIO_STUCK_TIMEOUT_MS = 300000; // 5 minutes
};

} // namespace lorapnp
