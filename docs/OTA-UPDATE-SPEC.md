# OTA Firmware Update Specification

## 1. Overview

Over-the-air firmware update system supporting full and delta updates across WiFi, BLE, and LoRa transports.

**Key Features:**
- Multi-transport support (WiFi, BLE, LoRa)
- Delta updates (send only changed bytes) for LoRa
- Resumable transfers
- Cryptographic verification
- Rollback on failure

## 2. Design Constraints

### 2.1 Transport Limitations

| Transport | Throughput | Update Time (512 KB) | Best For |
|-----------|-----------|---------------------|----------|
| **WiFi** | ~1 MB/s | ~1 second | Full firmware updates |
| **BLE** | ~50 KB/s | ~10 seconds | Full or delta updates |
| **LoRa** | ~0.5 KB/s | ~17 minutes (full), ~2-5 min (delta) | Delta updates only |

### 2.2 Flash Layout (ESP32-S3)

```
┌──────────────────────────────────────┐
│ 0x0000    Bootloader (64 KB)         │
├──────────────────────────────────────┤
│ 0x8000    Partition Table (4 KB)     │
├──────────────────────────────────────┤
│ 0x9000    NVS (20 KB)                │
├──────────────────────────────────────┤
│ 0x10000   App0 (OTA_0) (1.5 MB)      │  ← Current firmware
├──────────────────────────────────────┤
│ 0x190000  App1 (OTA_1) (1.5 MB)      │  ← Update partition
├──────────────────────────────────────┤
│ 0x310000  SPIFFS (704 KB)            │
└──────────────────────────────────────┘
```

**Available space:** 1.5 MB per partition
**Typical firmware size:** 500-800 KB
**OTA partition:** Switches between App0 and App1 on successful update

## 3. Update Types

### 3.1 Full Update

Transfer entire firmware binary.

**Use cases:**
- Major version changes
- First-time OTA setup
- Corruption recovery
- WiFi/BLE transports (fast enough)

**Pros:**
- Simple, robust
- No dependencies on existing firmware

**Cons:**
- Large data transfer (500+ KB)
- Impractical over LoRa (~17 minutes)

### 3.2 Delta Update (Binary Diff)

Transfer only changed bytes using binary diff algorithm.

**Use cases:**
- Minor version updates
- Bug fixes
- LoRa transport (bandwidth-constrained)

**Pros:**
- Small data transfer (typically 10-50 KB for patches)
- LoRa-feasible (~2-5 minutes)

**Cons:**
- Requires matching base firmware
- More complex error handling
- Higher CPU/RAM usage during patching

### 3.3 Delta Size Estimation

Typical delta sizes (based on ESP32 OTA experience):

| Change Type | Delta Size | Example |
|-------------|-----------|---------|
| Bug fix (single function) | 2-5 KB | Fix sensor reading logic |
| Feature addition | 10-30 KB | Add new sensor type |
| Library update | 20-80 KB | Update RadioLib version |
| Major refactor | 100+ KB | Use full update instead |

## 4. Delta Update Algorithm

### 4.1 Binary Diff Tool

Use **bsdiff** algorithm (industry standard):

```bash
# On build server
bsdiff old_firmware.bin new_firmware.bin patch.bin

# Metadata
echo "OLD_SHA256=$(sha256sum old_firmware.bin | cut -d' ' -f1)" > patch.meta
echo "NEW_SHA256=$(sha256sum new_firmware.bin | cut -d' ' -f1)" >> patch.meta
echo "PATCH_SIZE=$(stat -f%z patch.bin)" >> patch.meta
```

**Alternative:** For simpler implementation, use block-level diff:

```cpp
// Divide firmware into 4KB blocks
// Send only blocks that changed
constexpr uint32_t BLOCK_SIZE = 4096;

struct DeltaBlock {
    uint16_t block_index;    // Block number (0-based)
    uint8_t data[BLOCK_SIZE]; // New block data
    uint16_t crc16;          // CRC of block
};
```

### 4.2 Patch Application (On-Device)

```cpp
// Pseudocode for applying delta
bool applyDeltaPatch(const uint8_t* patch_data, size_t patch_len) {
    // Read current firmware from flash
    const esp_partition_t* running = esp_ota_get_running_partition();

    // Open OTA partition for writing
    const esp_partition_t* update = esp_ota_get_next_update_partition(NULL);
    esp_ota_handle_t ota_handle;
    esp_ota_begin(update, OTA_SIZE_UNKNOWN, &ota_handle);

    // Apply patch (bspatch-style or block-level)
    bspatch(running->address, patch_data, patch_len, ota_handle);

    // Finalize and verify
    esp_ota_end(ota_handle);
    esp_ota_set_boot_partition(update);

    return true;
}
```

**Memory requirement:**
- Block-level diff: ~8 KB RAM (2x block size)
- bspatch: ~30-50 KB RAM (depends on patch complexity)

## 5. Transfer Protocol

### 5.1 Message Types

Add new message types to PROTOCOL-SPEC.md:

| Value | Name | Direction | Description |
|-------|------|-----------|-------------|
| `0x20` | OTA_ANNOUNCE | Coordinator → Node | Announce available update |
| `0x21` | OTA_REQUEST | Node → Coordinator | Node requests update |
| `0x22` | OTA_START | Coordinator → Node | Begin transfer session |
| `0x23` | OTA_CHUNK | Coordinator → Node | Firmware data chunk |
| `0x24` | OTA_CHUNK_ACK | Node → Coordinator | Acknowledge chunk received |
| `0x25` | OTA_COMPLETE | Coordinator → Node | All chunks sent |
| `0x26` | OTA_VERIFY | Node → Coordinator | Verification result |
| `0x27` | OTA_ABORT | Both | Cancel update |

### 5.2 Update Flow

```
Node                           Coordinator
  │                                 │
  │◄──── OTA_ANNOUNCE ──────────────┤  "v1.2.0 available, delta 15KB"
  │                                 │
  │───── OTA_REQUEST ──────────────►│  "Ready to receive"
  │                                 │
  │◄──── OTA_START ─────────────────┤  session_id, total_chunks, chunk_size
  │                                 │
  │◄──── OTA_CHUNK (0) ─────────────┤
  ├───── OTA_CHUNK_ACK (0) ────────►│
  │                                 │
  │◄──── OTA_CHUNK (1) ─────────────┤
  ├───── OTA_CHUNK_ACK (1) ────────►│
  │                                 │
  │       ... (repeat) ...          │
  │                                 │
  │◄──── OTA_COMPLETE ──────────────┤
  │                                 │
  │  [Apply patch, verify SHA256]   │
  │                                 │
  ├───── OTA_VERIFY (success) ─────►│
  │                                 │
  │      [Reboot into new FW]       │
  │                                 │
  │◄──── (normal packets) ──────────┤  "Verify node is running new version"
```

### 5.3 Chunk Format

**OTA_CHUNK Payload:**

```cpp
struct OTAChunkPayload {
    uint8_t session_id;       // Session identifier (prevent mixing updates)
    uint16_t chunk_index;     // 0-based chunk number
    uint16_t total_chunks;    // Total number of chunks
    uint8_t chunk_size;       // Actual data size in this chunk (≤200 bytes)
    uint8_t data[200];        // Firmware/patch data
    uint16_t chunk_crc16;     // CRC of this chunk's data
};
```

**Chunk size:** 200 bytes per chunk (fits in LoRa 240-byte payload with overhead)

**LoRa Airtime Calculation (CRITICAL for duty cycle compliance):**

SF7/125kHz/CR4/5 at 915 MHz:
- 240-byte packet (OTA_CHUNK): ~370 ms airtime
- ACK packet (12 bytes): ~60 ms airtime
- Round-trip: ~430 ms
- **1% duty cycle limit:** 36 seconds airtime per hour
- **Max packets/hour:** 36000 ms / 430 ms = **83 packets**
- **Sustained rate:** 83 / 3600 = **0.023 packets/second**

**Realistic LoRa OTA timing:**
- 15 KB delta = 77 chunks
- At 0.023 chunks/sec: 77 / 0.023 = **3,347 seconds = 56 minutes**
- With overhead + retries: **60-90 minutes per node**

**Duty cycle budget strategy:**
```cpp
constexpr uint32_t LORA_AIRTIME_MS_PER_CHUNK = 430;  // 370 ms + 60 ms ACK
constexpr uint32_t DUTY_CYCLE_WINDOW_MS = 3600000;   // 1 hour
constexpr uint32_t DUTY_CYCLE_LIMIT_MS = 36000;      // 1% of hour

uint32_t airtime_budget_used_ms = 0;
uint32_t budget_window_start_ms = millis();

bool canSendOTAChunk() {
    uint32_t now = millis();

    // Reset budget every hour
    if (now - budget_window_start_ms > DUTY_CYCLE_WINDOW_MS) {
        airtime_budget_used_ms = 0;
        budget_window_start_ms = now;
    }

    // Check if we have budget
    if (airtime_budget_used_ms + LORA_AIRTIME_MS_PER_CHUNK > DUTY_CYCLE_LIMIT_MS) {
        ESP_LOGW("OTA", "Duty cycle limit reached, pausing OTA");
        return false;
    }

    return true;
}

void sendOTAChunkWithDutyCycle() {
    if (!canSendOTAChunk()) {
        // Wait until next budget window
        delay(60000); // Check again in 1 minute
        return;
    }

    sendOTAChunk();
    airtime_budget_used_ms += LORA_AIRTIME_MS_PER_CHUNK;

    // Minimum inter-chunk delay to spread traffic
    delay(43000); // 43 seconds between chunks for safety margin
}
```

**Recommendation:** For LoRa OTA, require nodes to be within BLE range (bring device physically closer to gateway) or use WiFi if available. LoRa OTA should be emergency-only.

### 5.4 Resume Support

Track transferred chunks in NVS:

```cpp
struct OTASessionState {
    uint32_t session_id;      // 32-bit nonce (not 8-bit)
    uint16_t total_chunks;
    uint16_t chunks_received;
    // Bitmap sized from manifest (dynamic allocation)
    // For 1.5 MB firmware: 7680 chunks @ 200 bytes = 960 bytes bitmap
    uint8_t* chunk_bitmap;    // Allocated based on total_chunks
    size_t bitmap_size;       // Actual bitmap size in bytes
    uint8_t firmware_sha256[32]; // Expected final hash
    uint32_t last_update_ms;  // For timeout detection
};

// CRITICAL: Store resume state in RAM, not NVS
// Only checkpoint to NVS every 100 chunks or on pause
// Writing per-chunk violates PROTOCOL-SPEC.md:318 (<1 write/min)
```

On interruption (power loss, connection drop):
- Save state to NVS
- On reconnect, send OTA_REQUEST with session_id and bitmap
- Coordinator resumes from missing chunks

## 6. Security & Verification

### 6.1 Cryptographic Signing (Required Phase 1)

**CRITICAL:** SHA256 checksums alone provide integrity but NOT authenticity. An attacker who can modify the update source can ship both malicious firmware and a matching hash. RSA signatures are REQUIRED from Phase 1.

**Build-time signing:**

```bash
# Generate signing key (once, keep private key OFFLINE)
openssl genrsa -out ota_private_key.pem 2048
openssl rsa -in ota_private_key.pem -pubout -out ota_public_key.pem

# Sign firmware binary
openssl dgst -sha256 -sign ota_private_key.pem -out firmware.sig firmware.bin

# Create signed manifest
cat > manifest.json <<EOF
{
  "version": "1.1.0",
  "firmware_sha256": "$(sha256sum firmware.bin | cut -d' ' -f1)",
  "signature": "$(base64 firmware.sig)"
}
EOF

# Sign manifest itself
openssl dgst -sha256 -sign ota_private_key.pem -out manifest.sig manifest.json
```

**Embed public key at compile time (platformio.ini):**

```ini
build_flags =
    -DOTA_PUBLIC_KEY_PEM=\"$(cat ota_public_key.pem | tr -d '\n')\"
```

**On-device verification (BEFORE writing to flash):**

```cpp
#include <mbedtls/pk.h>
#include <mbedtls/sha256.h>

// Embedded at compile time
extern const char ota_public_key_pem[] asm("_binary_ota_public_key_pem_start");

bool verifyFirmwareSignature(const uint8_t* firmware, size_t len,
                             const uint8_t* signature, size_t sig_len) {
    mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);

    // Load public key
    int ret = mbedtls_pk_parse_public_key(&pk,
        (const unsigned char*)ota_public_key_pem,
        strlen(ota_public_key_pem) + 1);
    if (ret != 0) {
        ESP_LOGE("OTA", "Failed to parse public key: %d", ret);
        mbedtls_pk_free(&pk);
        return false;
    }

    // Hash firmware
    uint8_t hash[32];
    mbedtls_sha256(firmware, len, hash, 0);

    // Verify signature
    ret = mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256,
        hash, sizeof(hash), signature, sig_len);

    mbedtls_pk_free(&pk);

    if (ret != 0) {
        ESP_LOGE("OTA", "Signature verification failed: %d", ret);
        return false;
    }

    return true;
}
```

**Verification flow:**
1. Download manifest + signature
2. Verify manifest signature with public key
3. Download firmware chunks
4. Verify firmware SHA256 matches manifest
5. Verify firmware signature with public key
6. Only then write to OTA partition

**Key management:**
- Private key stored OFFLINE (not in repo, not on coordinator)
- Public key embedded in firmware at build time
- Use hardware security module (HSM) for production signing

### 6.2 Rollback Protection

**Anti-rollback counter (optional):**

Store firmware version counter in NVS:

```cpp
uint32_t firmware_version_counter = 42; // Increment on each release

bool validateVersion(uint32_t new_version) {
    uint32_t stored = nvs_get_u32("fw_ver");
    return new_version >= stored; // Prevent downgrade attacks
}
```

**eFuse-based (permanent, use carefully):**

ESP32-S3 has eFuses for anti-rollback (cannot be reset).

### 6.3 Verification Steps

1. **SHA256 checksum** (mandatory): Verify patch integrity
2. **RSA signature** (recommended): Verify patch authenticity
3. **Version check** (optional): Prevent downgrade
4. **Post-update health check** (mandatory): Verify boot + basic functionality

## 7. Rollback Strategy

### 7.1 Automatic Rollback (ESP-IDF Native APIs)

**CRITICAL:** Use ESP-IDF OTA rollback APIs, not custom NVS flags. The bootloader does NOT automatically rollback based on custom flags.

```cpp
#include <esp_ota_ops.h>
#include <esp_system.h>

void setup() {
    // Get boot partition info
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;

    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            ESP_LOGI("OTA", "First boot after OTA, running diagnostics...");

            // Run actual health checks (NOT just reaching setup())
            bool healthy = performDiagnostics();

            if (healthy) {
                ESP_LOGI("OTA", "Diagnostics passed, marking app valid");
                esp_ota_mark_app_valid_cancel_rollback();
            } else {
                ESP_LOGE("OTA", "Diagnostics failed, rolling back");
                esp_ota_mark_app_invalid_rollback_and_reboot();
            }
        }
    }
}

// Real health check, not just "we booted"
bool performDiagnostics() {
    // 1. Radio initializes
    if (!radio.begin()) {
        ESP_LOGE("DIAG", "Radio init failed");
        return false;
    }

    // 2. Can read NVS
    if (!prefs.begin("lorapnp", false)) {
        ESP_LOGE("DIAG", "NVS read failed");
        return false;
    }

    // 3. Sufficient free heap
    if (esp_get_free_heap_size() < 50000) {
        ESP_LOGE("DIAG", "Low heap: %u", esp_get_free_heap_size());
        return false;
    }

    // 4. Can transmit/receive on LoRa (send test packet)
    if (!radioSelfTest()) {
        ESP_LOGE("DIAG", "Radio self-test failed");
        return false;
    }

    ESP_LOGI("DIAG", "All checks passed");
    return true;
}
```

**Rollback configuration (sdkconfig or menuconfig):**

```ini
CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y
CONFIG_BOOTLOADER_NUM_PIN_APP_TEST=0  # No GPIO test needed
```

**How ESP-IDF rollback works:**
1. After OTA, partition state is `ESP_OTA_IMG_PENDING_VERIFY`
2. Bootloader tracks boot attempts
3. If app calls `esp_ota_mark_app_valid_cancel_rollback()`, rollback is disabled
4. If app doesn't confirm within N reboots, bootloader reverts to previous partition
5. Configure rollback attempts in sdkconfig:

```ini
CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y
CONFIG_BOOTLOADER_APP_ANTI_ROLLBACK=n  # Don't use eFuse anti-rollback yet
```

### 7.2 Manual Rollback

Expose rollback command via web UI or BLE:

```cpp
// Message type: 0x28 = OTA_ROLLBACK
void handleRollbackRequest() {
    const esp_partition_t* last = esp_ota_get_last_invalid_partition();
    if (last != NULL) {
        esp_ota_set_boot_partition(last);
        esp_restart();
    }
}
```

## 8. Transport-Specific Optimizations

### 8.1 WiFi OTA

Use ESP32's built-in HTTP OTA:

```cpp
#include <HTTPUpdate.h>

void updateViaWiFi(const char* firmware_url) {
    t_httpUpdate_return ret = httpUpdate.update(wifiClient, firmware_url);

    if (ret == HTTP_UPDATE_OK) {
        Serial.println("Update success, rebooting...");
        ESP.restart();
    }
}
```

**Advantages:**
- Fast (~1-2 seconds for 500 KB)
- Reliable
- Built-in resume support

### 8.2 BLE OTA

Use Nordic DFU-style chunking:

```cpp
// Faster chunk size over BLE (up to 244 bytes per packet)
constexpr uint16_t BLE_OTA_CHUNK_SIZE = 240;

// Request next chunk immediately after ACK
// No artificial delays, maximize throughput
```

**Throughput optimization:**
- Use BLE connection interval = 7.5ms (minimum)
- Increase MTU to 512 bytes (negotiate with central)
- Disable BLE notifications during OTA

### 8.3 LoRa OTA

**Adaptive chunk rate:**

```cpp
// Adjust transmission rate based on packet loss
uint8_t consecutive_acks = 0;
uint16_t chunk_delay_ms = 2000; // Start conservative

void sendNextChunk() {
    sendOTAChunk();

    if (ack_received) {
        consecutive_acks++;
        if (consecutive_acks > 5) {
            // Increase speed
            chunk_delay_ms = max(500, chunk_delay_ms - 100);
        }
    } else {
        // Slow down
        chunk_delay_ms = min(5000, chunk_delay_ms + 500);
        consecutive_acks = 0;
    }

    delay(chunk_delay_ms);
}
```

**LoRa-specific optimizations:**
- Use SF7 (fastest) for OTA chunks if RSSI allows
- Increase TX power to 20 dBm during OTA (reduce packet loss)
- Disable deep sleep on node during OTA

## 9. Resource Usage

### 9.1 RAM Requirements

| Operation | RAM Usage |
|-----------|-----------|
| Block-level diff buffer | ~8 KB |
| bspatch buffer (if used) | ~30 KB |
| OTA partition handle | ~1 KB |
| Chunk buffer | ~256 bytes |
| Session state | ~512 bytes |
| **Total** | **~10-40 KB** |

### 9.2 Flash Writes

**Wear leveling:**
- OTA writes entire partition (~500 KB)
- ESP32 flash rated for 100,000 erase cycles
- Expected OTA frequency: 1x per week
- Flash lifetime: ~1,900 years (not a concern)

## 10. User Interface

### 10.1 Web Dashboard (Coordinator)

```html
<div id="ota-panel">
    <h3>Firmware Update</h3>
    <input type="file" id="firmware-file" accept=".bin">
    <button onclick="uploadFirmware()">Upload Firmware</button>

    <h4>Available Updates</h4>
    <div id="update-list">
        <!-- Populated dynamically -->
    </div>

    <h4>Node Update Status</h4>
    <table>
        <tr>
            <th>Node ID</th>
            <th>Current Version</th>
            <th>Target Version</th>
            <th>Progress</th>
            <th>Action</th>
        </tr>
        <!-- Rows populated for each node -->
    </table>
</div>
```

### 10.2 Progress Indicators

```cpp
struct OTAProgress {
    uint8_t node_id;
    uint16_t chunks_received;
    uint16_t total_chunks;
    uint8_t percent_complete; // 0-100
    uint32_t estimated_time_remaining_s;
    uint8_t transport_used; // 0=WiFi, 1=BLE, 2=LoRa
};
```

## 11. Testing Strategy

### 11.1 Delta Update Tests

- [ ] Create delta from v1.0 to v1.1 (minor change)
- [ ] Verify delta size < 50 KB
- [ ] Apply delta on test node, confirm successful boot
- [ ] Test with LoRa transport, measure transfer time
- [ ] Interrupt transfer at 50%, verify resume works
- [ ] Test with corrupted delta, verify rollback

### 11.2 Full Update Tests

- [ ] Full update via WiFi (<2 seconds)
- [ ] Full update via BLE (~10 seconds)
- [ ] Full update via LoRa (should recommend delta instead)
- [ ] Power loss during update → rollback
- [ ] Network drop during update → resume

### 11.3 Security Tests

- [ ] Modified firmware (bad signature) → rejected
- [ ] Downgrade attack (old version) → rejected
- [ ] Man-in-the-middle patch injection → detected via checksum

## 12. Build Process Integration

### 12.1 Automated Delta Generation

Add to CI/CD pipeline:

```yaml
# .github/workflows/build.yml
- name: Generate Delta Patch
  run: |
    # Download previous release firmware
    wget https://releases.example.com/firmware-v1.0.0.bin -O old.bin

    # Generate delta
    bsdiff old.bin .pio/build/lilygo-t3s3/firmware.bin patch.bin

    # Upload delta
    aws s3 cp patch.bin s3://ota-updates/v1.1.0-delta.bin
```

### 12.2 Version Manifest

Coordinator fetches update manifest from server:

```json
{
  "version": "1.1.0",
  "released": "2026-07-06",
  "full_update": {
    "url": "https://updates.example.com/firmware-v1.1.0.bin",
    "size": 524288,
    "sha256": "abc123..."
  },
  "delta_updates": [
    {
      "from_version": "1.0.0",
      "url": "https://updates.example.com/v1.0.0-to-v1.1.0.bin",
      "size": 15360,
      "sha256": "def456..."
    }
  ],
  "min_battery_v": 3.5,
  "changelog": "Fixed BLE reconnection bug, improved LoRa range"
}
```

## 13. Future Enhancements

### 13.1 Multicast OTA

Send single LoRa transmission to multiple nodes simultaneously:

```
Coordinator broadcasts:
  - OTA_CHUNK to dst=0xFFFF (broadcast)

All nodes:
  - Receive chunk, mark in bitmap
  - After transmission complete, request missing chunks individually
```

**Benefit:** Update 10 nodes in ~3 minutes instead of 30 minutes.

### 13.2 Delta Compression

Apply gzip or lz4 compression to delta patches:

```bash
bsdiff old.bin new.bin patch.bin
lz4 -9 patch.bin patch.lz4
# Typically achieves 30-50% additional size reduction
```

On-device, decompress before applying.

### 13.3 A/B Testing

Deploy new firmware to subset of nodes first:

```cpp
// Coordinator logic
if (node_id % 10 == 0) {
    // 10% of nodes get early access
    offerUpdate(node_id, "v1.2.0-beta");
}
```

## 14. Decisions Made

Based on project requirements:

1. **Delta Algorithm:** Block-level diff (4 KB blocks, ~8 KB RAM, good compression ratio)
2. **Security:** RSA-2048 signature verification required from Phase 1 (SHA256 alone is insufficient - attacker can modify both firmware and hash)
3. **LoRa Timing:** Realistic estimate 10-20 minutes for delta updates (duty cycle constrained)
4. **Update Trigger:** Manual approval required via web UI before starting update
5. **Rollback Policy:** Use ESP-IDF OTA APIs with proper diagnostic app validation
6. **Transport Priority:** Allow LoRa updates, but show warnings/failures in UI if packet loss high
7. **Remote Sensors:** Separate OTA strategy for BLE-only deep-sleep devices

## 15. Block-Level Delta Implementation

### 15.1 Algorithm Details

```cpp
constexpr uint32_t DELTA_BLOCK_SIZE = 4096; // 4 KB blocks
constexpr uint32_t MAX_FIRMWARE_SIZE = 1572864; // 1.5 MB
constexpr uint16_t MAX_BLOCKS = MAX_FIRMWARE_SIZE / DELTA_BLOCK_SIZE; // 384 blocks

struct DeltaBlockHeader {
    uint16_t total_blocks;     // Total blocks in firmware
    uint16_t changed_blocks;   // Number of blocks in this delta
    uint8_t base_fw_sha256[32]; // SHA256 of base firmware (verify compatibility)
    uint8_t new_fw_sha256[32];  // SHA256 of resulting firmware
};

struct DeltaBlock {
    uint16_t block_index;      // Which 4KB block (0-383)
    uint8_t data[4096];        // Block data
    uint16_t crc16;            // CRC of this block
};

// Build-time delta generation
void generateDelta(const char* old_fw, const char* new_fw, const char* delta_out) {
    FILE* old = fopen(old_fw, "rb");
    FILE* new = fopen(new_fw, "rb");
    FILE* delta = fopen(delta_out, "wb");

    DeltaBlockHeader header;
    header.total_blocks = (new_size + DELTA_BLOCK_SIZE - 1) / DELTA_BLOCK_SIZE;
    header.changed_blocks = 0;

    sha256_file(old_fw, header.base_fw_sha256);
    sha256_file(new_fw, header.new_fw_sha256);

    // Reserve space for header (fill in changed_blocks later)
    fwrite(&header, sizeof(header), 1, delta);

    // Compare blocks
    uint8_t old_block[DELTA_BLOCK_SIZE];
    uint8_t new_block[DELTA_BLOCK_SIZE];

    for (uint16_t i = 0; i < header.total_blocks; i++) {
        size_t old_read = fread(old_block, 1, DELTA_BLOCK_SIZE, old);
        size_t new_read = fread(new_block, 1, DELTA_BLOCK_SIZE, new);

        // Compare blocks
        if (old_read != new_read || memcmp(old_block, new_block, new_read) != 0) {
            DeltaBlock block;
            block.block_index = i;
            memcpy(block.data, new_block, new_read);
            block.crc16 = crc16_calc(block.data, new_read);

            fwrite(&block, sizeof(DeltaBlock), 1, delta);
            header.changed_blocks++;
        }
    }

    // Update header with actual changed_blocks count
    fseek(delta, 0, SEEK_SET);
    fwrite(&header, sizeof(header), 1, delta);

    fclose(old);
    fclose(new);
    fclose(delta);

    printf("Delta: %u / %u blocks changed (%.1f%%)\n",
           header.changed_blocks, header.total_blocks,
           100.0 * header.changed_blocks / header.total_blocks);
}

// On-device delta application (with proper bounds checking)
bool applyBlockDelta(const uint8_t* delta_data, size_t delta_len) {
    // CRITICAL: Validate buffer size before casting
    if (delta_len < sizeof(DeltaBlockHeader)) {
        ESP_LOGE("OTA", "Delta too small: %u bytes", delta_len);
        return false;
    }

    DeltaBlockHeader* header = (DeltaBlockHeader*)delta_data;

    // CRITICAL: Validate header fields
    if (header->total_blocks > MAX_BLOCKS) {
        ESP_LOGE("OTA", "Invalid total_blocks: %u (max %u)",
                 header->total_blocks, MAX_BLOCKS);
        return false;
    }
    if (header->changed_blocks > header->total_blocks) {
        ESP_LOGE("OTA", "Invalid changed_blocks: %u > %u",
                 header->changed_blocks, header->total_blocks);
        return false;
    }

    // CRITICAL: Validate delta buffer contains all promised blocks
    size_t expected_size = sizeof(DeltaBlockHeader) +
                          (header->changed_blocks * sizeof(DeltaBlock));
    if (delta_len < expected_size) {
        ESP_LOGE("OTA", "Delta truncated: %u < %u", delta_len, expected_size);
        return false;
    }

    // Verify base firmware matches
    uint8_t running_sha256[32];
    const esp_partition_t* running = esp_ota_get_running_partition();
    sha256_partition(running, running_sha256);

    if (memcmp(running_sha256, header->base_fw_sha256, 32) != 0) {
        ESP_LOGE("OTA", "Base firmware mismatch - delta incompatible");
        return false;
    }

    // Open OTA partition for writing
    const esp_partition_t* update = esp_ota_get_next_update_partition(NULL);
    esp_ota_handle_t ota_handle;
    esp_err_t err = esp_ota_begin(update, header->total_blocks * DELTA_BLOCK_SIZE,
                                   &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE("OTA", "esp_ota_begin failed: %d", err);
        return false;
    }

    // Strategy: Build complete firmware in OTA partition via esp_ota_write() only
    // Do NOT mix esp_ota_write() and esp_partition_write()

    DeltaBlock* changed = (DeltaBlock*)(delta_data + sizeof(DeltaBlockHeader));
    uint16_t changed_idx = 0;

    uint8_t block[DELTA_BLOCK_SIZE];
    for (uint16_t i = 0; i < header->total_blocks; i++) {
        // Check if this block was changed
        bool is_changed = false;
        if (changed_idx < header->changed_blocks &&
            changed[changed_idx].block_index == i) {

            // CRITICAL: Validate block_index is in bounds
            if (changed[changed_idx].block_index >= header->total_blocks) {
                ESP_LOGE("OTA", "Invalid block_index: %u >= %u",
                         changed[changed_idx].block_index, header->total_blocks);
                esp_ota_abort(ota_handle);
                return false;
            }

            // Verify CRC before using data
            uint16_t calc_crc = crc16_calc(changed[changed_idx].data, DELTA_BLOCK_SIZE);
            if (calc_crc != changed[changed_idx].crc16) {
                ESP_LOGE("OTA", "Block %u CRC mismatch", i);
                esp_ota_abort(ota_handle);
                return false;
            }

            memcpy(block, changed[changed_idx].data, DELTA_BLOCK_SIZE);
            changed_idx++;
            is_changed = true;
        } else {
            // Unchanged block - read from running partition
            esp_partition_read(running, i * DELTA_BLOCK_SIZE, block, DELTA_BLOCK_SIZE);
        }

        // Write block via OTA API (unified write path)
        err = esp_ota_write(ota_handle, block, DELTA_BLOCK_SIZE);
        if (err != ESP_OK) {
            ESP_LOGE("OTA", "esp_ota_write block %u failed: %d", i, err);
            esp_ota_abort(ota_handle);
            return false;
        }
    }

    // Finalize OTA
    err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE("OTA", "esp_ota_end failed: %d", err);
        return false;
    }

    // Verify final SHA256 AFTER finalization
    uint8_t result_sha256[32];
    sha256_partition(update, result_sha256);

    if (memcmp(result_sha256, header->new_fw_sha256, 32) != 0) {
        ESP_LOGE("OTA", "Final firmware SHA256 mismatch");
        return false;
    }

    // Set boot partition (ESP-IDF will mark as PENDING_VERIFY)
    err = esp_ota_set_boot_partition(update);
    if (err != ESP_OK) {
        ESP_LOGE("OTA", "esp_ota_set_boot_partition failed: %d", err);
        return false;
    }

    ESP_LOGI("OTA", "Delta applied successfully, reboot to activate");
    return true;
}
```

### 15.2 Expected Delta Sizes

Typical change scenarios:

| Change Type | Blocks Changed | Delta Size | LoRa Transfer Time |
|-------------|----------------|------------|-------------------|
| Bug fix (1-2 functions) | 1-3 blocks | 4-12 KB | ~1-2 minutes |
| Feature addition | 5-10 blocks | 20-40 KB | ~3-5 minutes |
| Library update | 10-20 blocks | 40-80 KB | ~5-10 minutes |
| Major refactor | 50+ blocks | 200+ KB | Use WiFi/BLE |

**Recommendation:** If delta >100 KB, UI should warn user to use WiFi/BLE if available.

---

## 16. Implementation Phases

**Phase 1 (MVP):**
- WiFi full updates using ESP32 HTTPUpdate library
- SHA256 verification
- Manual approval via web UI
- Manual rollback option

**Phase 2:**
- BLE full updates (chunked transfer)
- Block-level delta for LoRa (4 KB blocks)
- Auto-rollback after 3 boot failures
- Resume support for interrupted transfers

**Phase 3:**
- RSA-2048 signature verification
- Multicast LoRa OTA (update multiple nodes simultaneously)
- Delta compression (lz4 or gzip)
- A/B testing (staged rollouts)

## 17. UI Indicators for LoRa OTA

When updating over LoRa, show in web UI:

```javascript
{
  "node_id": 5,
  "update_status": "in_progress",
  "transport": "lora",
  "progress_percent": 45,
  "chunks_received": 35,
  "chunks_total": 77,
  "retries": 12,
  "estimated_time_remaining_s": 180,
  "warnings": [
    "LoRa packet loss: 15% (12 retries so far)",
    "Consider using BLE/WiFi for faster update"
  ]
}
```

**Failure threshold:** If retries exceed 50% of chunks, pause and recommend switching to BLE/WiFi.
