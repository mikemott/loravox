#pragma once
#include "Types.h"

namespace lorapnp {

struct Packet {
    uint8_t version;
    MessageType type;
    uint8_t flags;
    uint16_t src_id;
    uint16_t dst_id;
    uint16_t seq;
    uint8_t payload_len;
    uint8_t payload[MAX_PAYLOAD_SIZE];

    Packet();

    Status serialize(uint8_t* buf, size_t* buf_len) const;
    Status deserialize(const uint8_t* buf, size_t buf_len);

    bool flagSet(PacketFlag flag) const;
    void setFlag(PacketFlag flag);
    const char* typeStr() const;
};

} // namespace lorapnp
