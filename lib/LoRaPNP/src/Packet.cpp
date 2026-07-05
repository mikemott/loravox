#include "Packet.h"
#include "CRC.h"
#include <string.h>

namespace lorapnp {

Packet::Packet()
    : version(PROTOCOL_VERSION)
    , type(MessageType::DISCOVER)
    , flags(0)
    , src_id(0)
    , dst_id(0)
    , seq(0)
    , payload_len(0) {
    memset(payload, 0, MAX_PAYLOAD_SIZE);
}

Status Packet::serialize(uint8_t* buf, size_t* buf_len) const {
    if (!buf || !buf_len) return Status::ERR_SIZE;

    size_t pos = 0;
    buf[pos++] = version;
    buf[pos++] = static_cast<uint8_t>(type);
    buf[pos++] = flags;
    buf[pos++] = (src_id >> 8) & 0xFF;
    buf[pos++] = src_id & 0xFF;
    buf[pos++] = (dst_id >> 8) & 0xFF;
    buf[pos++] = dst_id & 0xFF;
    buf[pos++] = (seq >> 8) & 0xFF;
    buf[pos++] = seq & 0xFF;
    buf[pos++] = payload_len;

    if (payload_len > 0) {
        memcpy(&buf[pos], payload, payload_len);
        pos += payload_len;
    }

    uint16_t crc_val = crc16(buf, pos);
    buf[pos++] = (crc_val >> 8) & 0xFF;
    buf[pos++] = crc_val & 0xFF;

    *buf_len = pos;
    return Status::OK;
}

Status Packet::deserialize(const uint8_t* buf, size_t buf_len) {
    if (buf_len < HEADER_SIZE + 2) return Status::ERR_SIZE;

    version = buf[0];
    if (version != PROTOCOL_VERSION) return Status::ERR_VERSION;

    type = static_cast<MessageType>(buf[1]);
    flags = buf[2];
    src_id = ((uint16_t)buf[3] << 8) | buf[4];
    dst_id = ((uint16_t)buf[5] << 8) | buf[6];
    seq = ((uint16_t)buf[7] << 8) | buf[8];
    payload_len = buf[9];

    if (payload_len > MAX_PAYLOAD_SIZE) return Status::ERR_SIZE;
    if (buf_len < (size_t)(HEADER_SIZE + payload_len + 2)) return Status::ERR_SIZE;

    if (payload_len > 0) {
        memcpy(payload, &buf[HEADER_SIZE], payload_len);
    }

    size_t crc_offset = HEADER_SIZE + payload_len;
    uint16_t received_crc = ((uint16_t)buf[crc_offset] << 8) | buf[crc_offset + 1];

    uint16_t computed_crc = crc16(buf, crc_offset);
    if (computed_crc != received_crc) return Status::ERR_CRC;

    return Status::OK;
}

bool Packet::flagSet(PacketFlag flag) const {
    return (flags & static_cast<uint8_t>(flag)) != 0;
}

void Packet::setFlag(PacketFlag flag) {
    flags |= static_cast<uint8_t>(flag);
}

const char* Packet::typeStr() const {
    switch (type) {
        case MessageType::DISCOVER:      return "DISCOVER";
        case MessageType::OFFER:         return "OFFER";
        case MessageType::JOIN_REQUEST:  return "JOIN_REQ";
        case MessageType::JOIN_ACCEPT:   return "JOIN_ACC";
        case MessageType::DATA:          return "DATA";
        case MessageType::ACK:           return "ACK";
        case MessageType::CONFIG_UPDATE: return "CFG_UPD";
        case MessageType::CONFIG_ACK:    return "CFG_ACK";
        case MessageType::STATUS:        return "STATUS";
        case MessageType::SENSOR_DESCRIPTOR: return "SENSOR_DESC";
        case MessageType::SENSOR_DATA:   return "SENSOR_DATA";
        case MessageType::BLE_SENSOR_DATA: return "BLE_SENSOR";
        case MessageType::NAME_UPDATE:   return "NAME_UPD";
        case MessageType::REBOOT:        return "REBOOT";
        default:                         return "UNKNOWN";
    }
}

} // namespace lorapnp
