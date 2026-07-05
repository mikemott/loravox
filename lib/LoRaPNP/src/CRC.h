#pragma once
#include <stdint.h>
#include <stddef.h>

namespace lorapnp {

uint16_t crc16(const uint8_t* data, size_t length);

} // namespace lorapnp
