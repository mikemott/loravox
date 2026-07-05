#include "Radio.h"

namespace lorapnp {

Radio::Radio(uint8_t cs, uint8_t dio0, uint8_t rst, uint8_t dio1,
             uint8_t sck, uint8_t miso, uint8_t mosi)
    : cs_(cs), dio0_(dio0), rst_(rst), dio1_(dio1)
    , sck_(sck), miso_(miso), mosi_(mosi)
    , module_(nullptr), radio_(nullptr)
    , rssi_(0), snr_(0.0f)
    , consecutive_failures_(0), last_successful_op_ms_(0) {
    memset(tx_buf_, 0, sizeof(tx_buf_));
    memset(rx_buf_, 0, sizeof(rx_buf_));
}

Status Radio::init(const RadioConfig& config) {
    SPI.begin(sck_, miso_, mosi_, cs_);

    module_ = new Module(cs_, dio0_, rst_, dio1_);
    radio_ = new SX1276(module_);

    int16_t state = radio_->begin(
        config.frequency,
        config.bandwidth,
        config.sf,
        config.sync_word,
        config.tx_power
    );

    if (state != RADIOLIB_ERR_NONE) {
        delete radio_;
        delete module_;
        radio_ = nullptr;
        module_ = nullptr;
        return Status::ERR_RADIO;
    }

    config_ = config;
    consecutive_failures_ = 0;
    last_successful_op_ms_ = millis();

    return Status::OK;
}

Status Radio::sendPacket(const Packet& pkt) {
    size_t len = 0;
    Status s = pkt.serialize(tx_buf_, &len);
    if (s != Status::OK) return s;

    int16_t state = radio_->transmit(tx_buf_, len);
    if (state != RADIOLIB_ERR_NONE) {
        recordOperationFailure();
        return Status::ERR_RADIO;
    }

    recordOperationSuccess();
    return Status::OK;
}

Status Radio::receivePacket(Packet& pkt, uint32_t timeout_ms) {
    radio_->startReceive();

    uint32_t start = millis();
    while (millis() - start < timeout_ms) {
        if (digitalRead(dio0_) == HIGH) {
            size_t len = radio_->getPacketLength();
            if (len > 0 && len <= sizeof(rx_buf_)) {
                int16_t state = radio_->readData(rx_buf_, len);
                if (state == RADIOLIB_ERR_NONE) {
                    rssi_ = radio_->getRSSI();
                    snr_ = radio_->getSNR();
                    recordOperationSuccess();
                    return pkt.deserialize(rx_buf_, len);
                }
            }
            return Status::ERR_SIZE;
        }
        yield();
    }

    return Status::ERR_TIMEOUT;
}

bool Radio::needsReset() const {
    // Only reset if we have multiple consecutive failures
    // AND it's been a long time since last success (indicates stuck radio)
    if (consecutive_failures_ < RADIO_FAILURE_THRESHOLD) {
        return false;
    }

    uint32_t now = millis();
    uint32_t time_since_success = now - last_successful_op_ms_;
    return time_since_success > RADIO_STUCK_TIMEOUT_MS;
}

Status Radio::reset() {
    if (radio_) {
        radio_->standby();
        delay(10);
        delete radio_;
        delete module_;
        radio_ = nullptr;
        module_ = nullptr;
    }

    module_ = new Module(cs_, dio0_, rst_, dio1_);
    radio_ = new SX1276(module_);

    int16_t state = radio_->begin(
        config_.frequency,
        config_.bandwidth,
        config_.sf,
        config_.sync_word,
        config_.tx_power
    );

    if (state != RADIOLIB_ERR_NONE) {
        delete radio_;
        delete module_;
        radio_ = nullptr;
        module_ = nullptr;
        return Status::ERR_RADIO;
    }

    consecutive_failures_ = 0;
    last_successful_op_ms_ = millis();

    return Status::OK;
}

void Radio::recordOperationFailure() {
    consecutive_failures_++;
}

void Radio::recordOperationSuccess() {
    consecutive_failures_ = 0;
    last_successful_op_ms_ = millis();
}

uint32_t Radio::getConsecutiveFailures() const {
    return consecutive_failures_;
}

} // namespace lorapnp
