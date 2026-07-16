#pragma once
#include <Arduino.h>
#include "Types.h"

namespace lorapnp {

// Derive the stable 6-byte device UID from the ESP32 eFuse MAC (big-endian).
// Shared by every field device so identity is computed identically everywhere
// (node/relay build and remote-sensor/leaf build previously had their own copies).
// Inline: costs nothing in a firmware that never calls it (e.g. the gateway).
inline void loadDeviceUid(uint8_t uid[DEVICE_UID_LEN]) {
  uint64_t mac = ESP.getEfuseMac();
  for (uint8_t i = 0; i < DEVICE_UID_LEN; i++) {
    uid[i] = (mac >> (8 * (DEVICE_UID_LEN - 1 - i))) & 0xFF;
  }
}

} // namespace lorapnp
