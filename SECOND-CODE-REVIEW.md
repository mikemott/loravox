# Second Code Review - Critical Issues Remaining

**Date**: 2026-07-06
**Reviewer**: External code review
**Status**: PR #2 has 1 blocker, PR #3 has 4 critical/high issues

---

## Executive Summary

While the first round of code review fixes (documented in CODE-REVIEW-FIXES.md) addressed many issues, this second review reveals **remaining critical problems** that must be fixed before merge:

- **PR #2**: 1 blocker (wifi off command inconsistency)
- **PR #3**: 2 critical + 2 high priority issues (memory exhaustion, hash verification bug, incomplete resume, placeholder key)

---

## PR #2: Network Selection - 1 Blocker

### Blocker: `wifi off` Command Doesn't Use NetworkManager API

**File**: src/main.cpp:2217-2222
**Severity**: BLOCKER
**Impact**: WiFi can remain enabled and re-enable on reboot even after `wifi off`

**Current Code**:
```cpp
} else if (cmd == "wifi off") {
  wifi_sta_enabled = false;
  prefs.begin("lorapnp", false);
  prefs.putBool("wifi_sta_en", false);
  prefs.end();
  Serial.println("WiFi station mode disabled. Restart to apply.");
```

**Problem**:
1. Only updates the old `lorapnp/wifi_sta_en` preference key
2. Does NOT call `network.enableWiFi(false)`
3. NetworkManager persists its own `network/wifi_enabled` key
4. On reboot, NetworkManager reads `network/wifi_enabled` (still true) and re-enables WiFi
5. The `wifi` command (for setting credentials) was fixed in the first review to use NetworkManager, but `wifi off` was missed

**Required Fix**:
```cpp
} else if (cmd == "wifi off") {
  // Use NetworkManager API for consistency
  network.enableWiFi(false);

  // Keep old flag in sync for compatibility
  wifi_sta_enabled = false;
  prefs.begin("lorapnp", false);
  prefs.putBool("wifi_sta_en", false);
  prefs.end();

  Serial.println("WiFi disabled.");
}
```

**Verdict**: PR #2 is close but needs this fix before merge.

---

## PR #3: OTA Updates - 4 Critical/High Issues

### Critical Issue #1: Signature Verification Exhausts Memory

**File**: lib/LoRaPNP/src/OTAManager.cpp:283 (in finalizeUpdate)
**Severity**: CRITICAL
**Impact**: Signed OTA will always fail on ESP32-S3 targets

**Problem**:
The first code review "fix" introduced this bug. In CODE-REVIEW-FIXES.md Issue #2, RSA verification was added:

```cpp
// OTAManager.cpp:240 (finalizeUpdate)
// ✅ Read firmware and verify signature
uint8_t* firmware_buffer = (uint8_t*)malloc(session.firmware_size);
esp_partition_read(update_partition, 0, firmware_buffer, session.firmware_size);

bool sig_valid = verifyFirmwareSignature(firmware_buffer, session.firmware_size,
                                         signature, sig_len);
free(firmware_buffer);
```

**Why This Fails**:
- Typical firmware size: ~900 KB
- ESP32-S3 available RAM: ~320 KB
- `malloc(900000)` will FAIL
- Signed OTA cannot complete

**Root Cause in verifyFirmwareSignature()**:
```cpp
// Line ~345
bool OTAManager::verifyFirmwareSignature(const uint8_t* firmware, size_t len,
                                         const uint8_t* signature, size_t sig_len) {
    // Hash firmware
    uint8_t hash[32];
    mbedtls_sha256(firmware, len, hash, 0);  // ❌ Requires entire firmware in RAM

    // Verify signature
    mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256, hash, sizeof(hash), signature, sig_len);
}
```

**Required Fix**:
Stream the firmware from flash in chunks and compute the hash incrementally, just like `verifyFinalHash()` does:

```cpp
bool OTAManager::verifyFirmwareSignature(const uint8_t* signature, size_t sig_len) {
    // Hash firmware in chunks (streaming)
    uint8_t hash[32];
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);

    const size_t chunk_size = 4096;
    uint8_t* buffer = (uint8_t*)malloc(chunk_size);  // Only 4KB needed
    if (!buffer) return false;

    size_t total_read = 0;
    for (size_t offset = 0; offset < session.firmware_size; offset += chunk_size) {
        size_t read_size = min(chunk_size, session.firmware_size - offset);

        esp_err_t err = esp_partition_read(update_partition, offset, buffer, read_size);
        if (err != ESP_OK) {
            free(buffer);
            return false;
        }

        mbedtls_sha256_update(&ctx, buffer, read_size);
        total_read += read_size;
    }

    mbedtls_sha256_finish(&ctx, hash);
    mbedtls_sha256_free(&ctx);
    free(buffer);

    // Now verify signature against computed hash
    mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);

    int ret = mbedtls_pk_parse_public_key(&pk,
        (const unsigned char*)ota_public_key_pem,
        strlen(ota_public_key_pem) + 1);

    if (ret != 0) {
        mbedtls_pk_free(&pk);
        return false;
    }

    ret = mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256,
        hash, sizeof(hash), signature, sig_len);

    mbedtls_pk_free(&pk);
    return (ret == 0);
}
```

**Also update finalizeUpdate()**:
```cpp
bool OTAManager::finalizeUpdate(const uint8_t* signature, size_t sig_len) {
    // ...verify hash...

    // ✅ Streaming signature verification (no large allocation)
    if (!verifyFirmwareSignature(signature, sig_len)) {
        ESP_LOGE(TAG, "RSA signature verification FAILED");
        return false;
    }

    // ...set boot partition...
}
```

---

### Critical Issue #2: verifyFinalHash() Reads Past Firmware End

**File**: lib/LoRaPNP/src/OTAManager.cpp:451
**Severity**: CRITICAL
**Impact**: Hash verification fails for most valid firmware (non-4096-aligned sizes)

**Current Code**:
```cpp
bool OTAManager::verifyFinalHash() {
    const size_t chunk_size = 4096;
    uint8_t* buffer = (uint8_t*)malloc(chunk_size);

    size_t total_read = 0;
    size_t partition_size = update_partition->size;

    for (size_t offset = 0; offset < partition_size; offset += chunk_size) {
        size_t read_size = min(chunk_size, partition_size - offset);

        esp_partition_read(update_partition, offset, buffer, read_size);

        mbedtls_sha256_update(&ctx, buffer, read_size);
        total_read += read_size;

        // ❌ BUG: Checked AFTER hashing the chunk
        if (total_read >= session.firmware_size) {
            break;
        }
    }
}
```

**Problem**:
When firmware size is NOT a multiple of 4096 bytes (e.g., 921,473 bytes):
1. Last iteration: `offset = 921,600`, `read_size = 4096`
2. Hash `4096 bytes` (including 4096 - 127 = 3969 bytes of garbage after firmware end)
3. THEN check `total_read >= firmware_size` and break
4. Hash includes extra data → verification fails

**Required Fix**:
```cpp
bool OTAManager::verifyFinalHash() {
    const size_t chunk_size = 4096;
    uint8_t* buffer = (uint8_t*)malloc(chunk_size);

    size_t total_read = 0;

    // ✅ Loop until we've read exactly firmware_size bytes
    while (total_read < session.firmware_size) {
        size_t remaining = session.firmware_size - total_read;
        size_t read_size = min(chunk_size, remaining);  // ✅ Never exceed firmware_size

        esp_err_t err = esp_partition_read(update_partition, total_read, buffer, read_size);
        if (err != ESP_OK) {
            free(buffer);
            return false;
        }

        mbedtls_sha256_update(&ctx, buffer, read_size);
        total_read += read_size;
    }

    // total_read now equals firmware_size exactly
    mbedtls_sha256_finish(&ctx, calculated_hash);
    free(buffer);

    return (memcmp(calculated_hash, session.firmware_sha256, 32) == 0);
}
```

---

### High Priority Issue #3: Resume Incomplete

**File**: lib/LoRaPNP/src/OTAManager.cpp:511 (loadSessionFromNVS)
**Severity**: HIGH
**Impact**: Cannot safely resume OTA after power loss or reboot

**Current Code**:
```cpp
bool OTAManager::loadSessionFromNVS() {
    session.session_id = prefs.getUInt("session_id", 0);
    session.total_chunks = prefs.getUShort("total_chunks", 0);
    session.chunks_received = prefs.getUShort("chunks_rx", 0);
    session.state = OTA_DOWNLOADING;
    session.transport = (OTATransport)prefs.getUChar("transport", OTA_TRANSPORT_LORA);
    prefs.getBytes("fw_sha256", session.firmware_sha256, 32);

    // Reallocate bitmap
    session.bitmap_size = (session.total_chunks + 7) / 8;
    session.chunk_bitmap = (uint8_t*)malloc(session.bitmap_size);
    if (session.chunk_bitmap) {
        prefs.getBytes("bitmap", session.chunk_bitmap, session.bitmap_size);
    }

    // ❌ firmware_size NOT restored
    // ❌ chunk_size NOT restored
    // ❌ esp_ota_handle_t NOT restored
}
```

**Problems**:
1. **firmware_size = 0**: `verifyFinalHash()` will hash 0 bytes
2. **chunk_size = 0**: `processChunk()` cannot validate chunk sizes
3. **ota_handle invalid**: `esp_ota_write()` will fail or corrupt data
4. Even if chunks arrive, OTA cannot continue safely

**Required Fix**:

First, add fields to NVS save/load:
```cpp
bool OTAManager::saveSessionToNVS() {
    prefs.putUInt("session_id", session.session_id);
    prefs.putUShort("total_chunks", session.total_chunks);
    prefs.putUShort("chunks_rx", session.chunks_received);
    prefs.putUChar("transport", (uint8_t)session.transport);
    prefs.putBytes("fw_sha256", session.firmware_sha256, 32);

    // ✅ Add missing fields
    prefs.putUInt("fw_size", session.firmware_size);
    prefs.putUShort("chunk_size", session.chunk_size);

    if (session.chunk_bitmap) {
        prefs.putBytes("bitmap", session.chunk_bitmap, session.bitmap_size);
    }
}

bool OTAManager::loadSessionFromNVS() {
    session.session_id = prefs.getUInt("session_id", 0);
    if (session.session_id == 0) return false;

    session.total_chunks = prefs.getUShort("total_chunks", 0);
    session.chunks_received = prefs.getUShort("chunks_rx", 0);
    session.state = OTA_DOWNLOADING;
    session.transport = (OTATransport)prefs.getUChar("transport", OTA_TRANSPORT_LORA);
    prefs.getBytes("fw_sha256", session.firmware_sha256, 32);

    // ✅ Restore missing fields
    session.firmware_size = prefs.getUInt("fw_size", 0);
    session.chunk_size = prefs.getUShort("chunk_size", 0);

    // Reallocate bitmap
    session.bitmap_size = (session.total_chunks + 7) / 8;
    session.chunk_bitmap = (uint8_t*)malloc(session.bitmap_size);
    if (session.chunk_bitmap) {
        prefs.getBytes("bitmap", session.chunk_bitmap, session.bitmap_size);
    }

    // ✅ Reinitialize OTA handle
    // Note: This requires using esp_partition_write() instead of esp_ota_write()
    // for true resume support, because esp_ota_handle_t cannot be persisted.
    // For now, resume is limited - document this clearly.

    ESP_LOGW(TAG, "Session resumed but out-of-order chunks not supported");
    ESP_LOGW(TAG, "Expecting chunk %u next", session.chunks_received);

    return true;
}
```

**Important Note**: Full resume support requires switching from `esp_ota_write()` to `esp_partition_write()` because:
- `esp_ota_handle_t` is an opaque handle that cannot be serialized/restored
- Current implementation only supports sequential chunks
- True resume requires out-of-order chunk support (buffering)

**Recommended approach**: Document this limitation clearly and defer full resume to Phase 2.

---

### High Priority Issue #4: Placeholder Public Key

**File**: lib/LoRaPNP/src/OTAManager.cpp:9
**Severity**: HIGH
**Impact**: RSA verification cannot succeed for real updates

**Current Code**:
```cpp
// Placeholder public key - will be replaced with build-time embedded key
const char* OTAManager::ota_public_key_pem =
    "-----BEGIN PUBLIC KEY-----\n"
    "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA...\n"  // ❌ Truncated
    "-----END PUBLIC KEY-----\n";
```

**Problem**:
- This is not a valid PEM-encoded RSA public key
- The key is truncated (indicated by `...`)
- All signature verifications will fail
- Updates cannot be deployed

**Required Fix**:

1. **Generate a real RSA-2048 key pair**:
```bash
# Generate private key (keep SECRET, used for signing)
openssl genrsa -out firmware_signing_key.pem 2048

# Extract public key (embed in firmware)
openssl rsa -in firmware_signing_key.pem -pubout -out firmware_public_key.pem
```

2. **Embed the public key at build time**:
Create `tools/embed_public_key.py`:
```python
#!/usr/bin/env python3
import sys

def embed_key(pem_file, output_file):
    with open(pem_file, 'r') as f:
        key = f.read()

    with open(output_file, 'w') as f:
        f.write('#ifndef OTA_PUBLIC_KEY_H\n')
        f.write('#define OTA_PUBLIC_KEY_H\n\n')
        f.write('const char* OTA_PUBLIC_KEY_PEM = R"(\n')
        f.write(key)
        f.write(')";\n\n')
        f.write('#endif // OTA_PUBLIC_KEY_H\n')

if __name__ == '__main__':
    embed_key('firmware_public_key.pem', 'lib/LoRaPNP/src/OTAPublicKey.h')
```

3. **Add build step** to platformio.ini:
```ini
[env:gateway]
extra_scripts =
    pre:tools/embed_public_key.py
```

4. **Include the generated header**:
```cpp
// OTAManager.cpp
#include "OTAPublicKey.h"  // Auto-generated at build time

// Remove hardcoded placeholder
```

**Alternative** (simpler, less secure):
Just paste a real public key for now:
```cpp
const char* OTAManager::ota_public_key_pem =
    "-----BEGIN PUBLIC KEY-----\n"
    "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAyVZ9...\n"  // Full key
    "...VxyzABCD...\n"  // All lines
    "-----END PUBLIC KEY-----\n";
```

But note: The private key must be kept secure and used to sign firmware before deployment.

---

## Verdict

### PR #2: Network Selection
**Status**: ❌ **NOT READY**
**Blockers**: 1
**Action Required**: Fix `wifi off` command to call `network.enableWiFi(false)`

### PR #3: OTA Updates
**Status**: ❌ **NOT READY**
**Blockers**: 4 (2 critical, 2 high)
**Action Required**:
1. Fix signature verification to use streaming hash (no 900KB malloc)
2. Fix verifyFinalHash() to not read past firmware_size
3. Add firmware_size and chunk_size to NVS resume
4. Replace placeholder public key with real key

---

## Recommended Path Forward

1. **Fix PR #2 blocker** (10 minutes)
   - One-line change to call `network.enableWiFi(false)`
   - Test with `wifi off` command
   - Verify WiFi stays disabled on reboot

2. **Fix PR #3 critical issues** (2-3 hours)
   - Refactor signature verification for streaming (reuse verifyFinalHash pattern)
   - Fix hash loop boundary condition
   - Add NVS fields for resume
   - Generate and embed a real RSA key

3. **Test thoroughly**
   - PR #2: WiFi on/off cycles, reboots, transport switching
   - PR #3: Full firmware update via WiFi, signature verification, hash verification

4. **Document limitations clearly**
   - PR #3: Resume only works for sequential chunks (Phase 1)
   - PR #3: Out-of-order chunks rejected (Phase 2 needed)
   - Add "Known Limitations" section to PR description

---

**After these fixes, both PRs will be ready for hardware testing.**
