#include "OTAManager.h"
#include <esp_system.h>
#include <esp_log.h>

namespace lorapnp {

static const char* TAG = "OTAManager";

// CRITICAL: This is a PLACEHOLDER public key and MUST be replaced before production use.
// RSA signature verification will FAIL with this truncated key.
//
// To generate a real RSA-2048 key pair:
//   1. Generate private key (KEEP SECRET): openssl genrsa -out firmware_signing_key.pem 2048
//   2. Extract public key (embed here):    openssl rsa -in firmware_signing_key.pem -pubout -out firmware_public_key.pem
//   3. Copy the full PEM contents from firmware_public_key.pem and replace below
//
// For production, implement build-time key embedding via platformio.ini extra_scripts.
const char* OTAManager::ota_public_key_pem =
    "-----BEGIN PUBLIC KEY-----\n"
    "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA...\n"  // PLACEHOLDER - REPLACE ME
    "-----END PUBLIC KEY-----\n";

OTAManager::OTAManager()
    : lora_airtime_used_ms(0)
    , lora_window_start_ms(0)
    , last_chunk_sent_ms(0)
    , ota_handle(0)
    , update_partition(nullptr)
    , ota_begun(false)
{
    memset(&session, 0, sizeof(session));
    session.state = OTA_IDLE;
}

OTAManager::~OTAManager() {
    end();
}

bool OTAManager::begin() {
    ESP_LOGI(TAG, "OTA Manager initializing");

    // Get update partition info
    update_partition = esp_ota_get_next_update_partition(NULL);
    if (!update_partition) {
        ESP_LOGE(TAG, "No OTA partition found");
        return false;
    }

    ESP_LOGI(TAG, "OTA partition: %s, size=%u",
             update_partition->label, update_partition->size);

    // Check if we're in PENDING_VERIFY state (first boot after OTA)
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;

    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            ESP_LOGI(TAG, "First boot after OTA, running diagnostics...");

            // Perform health checks
            bool healthy = performDiagnostics();

            if (healthy) {
                ESP_LOGI(TAG, "Diagnostics passed, marking app valid");
                esp_ota_mark_app_valid_cancel_rollback();
            } else {
                ESP_LOGE(TAG, "Diagnostics failed, rolling back");
                esp_ota_mark_app_invalid_rollback_and_reboot();
                return false;  // Won't reach here
            }
        }
    }

    // Try to resume interrupted session
    if (loadSessionFromNVS()) {
        ESP_LOGI(TAG, "Resumed OTA session %u (%u/%u chunks)",
                 session.session_id, session.chunks_received, session.total_chunks);
    }

    return true;
}

void OTAManager::end() {
    if (ota_begun) {
        esp_ota_abort(ota_handle);
        ota_begun = false;
    }

    if (session.chunk_bitmap) {
        free(session.chunk_bitmap);
        session.chunk_bitmap = nullptr;
    }

    resetSession();
}

void OTAManager::update() {
    // Update LoRa airtime budget tracking
    updateAirtimeBudget();

    // Check for session timeout (5 minutes of inactivity)
    if (session.state == OTA_DOWNLOADING) {
        if (millis() - session.last_update_ms > 300000) {
            ESP_LOGW(TAG, "OTA session timeout, aborting");
            abortUpdate();
        }
    }

    // Periodic checkpoint (every 100 chunks or 30 seconds)
    if (session.state == OTA_DOWNLOADING) {
        static uint32_t last_checkpoint = 0;
        if (millis() - last_checkpoint > 30000) {
            saveSessionCheckpoint();
            last_checkpoint = millis();
        }
    }
}

bool OTAManager::startUpdate(uint32_t session_id, uint16_t total_chunks,
                             const uint8_t* firmware_sha256, OTATransport transport,
                             uint32_t firmware_size, uint16_t chunk_size) {
    if (session.state != OTA_IDLE && session.state != OTA_ANNOUNCED) {
        ESP_LOGW(TAG, "OTA already in progress");
        return false;
    }

    // Validate parameters
    // For LoRa chunks ~200 bytes: 900KB firmware = 4608 chunks (reasonable)
    // For WiFi/BLE full image: chunks = blocks = 219 (900KB / 4KB)
    if (total_chunks == 0 || total_chunks > 8192) {
        ESP_LOGE(TAG, "Invalid total_chunks: %u (max 8192)", total_chunks);
        return false;
    }

    if (firmware_size == 0 || firmware_size > MAX_FIRMWARE_SIZE) {
        ESP_LOGE(TAG, "Invalid firmware_size: %u (max %u)", firmware_size, MAX_FIRMWARE_SIZE);
        return false;
    }

    if (chunk_size == 0 || chunk_size > 4096) {
        ESP_LOGE(TAG, "Invalid chunk_size: %u (max 4096)", chunk_size);
        return false;
    }

    ESP_LOGI(TAG, "Starting OTA session %u: %u chunks (%u bytes each, %u total) via %s",
             session_id, total_chunks, chunk_size, firmware_size,
             (transport == OTA_TRANSPORT_WIFI) ? "WiFi" :
             (transport == OTA_TRANSPORT_BLE) ? "BLE" : "LoRa");

    // Initialize session
    resetSession();
    session.session_id = session_id;
    session.total_chunks = total_chunks;
    session.chunks_received = 0;
    session.transport = transport;
    session.state = OTA_DOWNLOADING;
    session.last_update_ms = millis();
    session.firmware_size = firmware_size;  // CRITICAL: Set firmware size for hash verification
    session.chunk_size = chunk_size;        // CRITICAL: Set chunk size for positioning
    memcpy(session.firmware_sha256, firmware_sha256, 32);

    // Allocate chunk bitmap
    session.bitmap_size = (total_chunks + 7) / 8;  // Round up to bytes
    session.chunk_bitmap = (uint8_t*)calloc(session.bitmap_size, 1);
    if (!session.chunk_bitmap) {
        ESP_LOGE(TAG, "Failed to allocate chunk bitmap (%u bytes)", session.bitmap_size);
        resetSession();
        return false;
    }

    // Begin OTA
    esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed: %d", err);
        free(session.chunk_bitmap);
        session.chunk_bitmap = nullptr;
        resetSession();
        return false;
    }

    ota_begun = true;

    // Save initial checkpoint
    saveSessionToNVS();

    return true;
}

bool OTAManager::processChunk(uint16_t chunk_index, const uint8_t* data, size_t len) {
    if (session.state != OTA_DOWNLOADING) {
        ESP_LOGW(TAG, "Not in downloading state");
        return false;
    }

    // Validate chunk index
    if (chunk_index >= session.total_chunks) {
        ESP_LOGE(TAG, "Invalid chunk index: %u (max %u)",
                 chunk_index, session.total_chunks);
        return false;
    }

    // Check if already received
    if (isChunkReceived(chunk_index)) {
        ESP_LOGD(TAG, "Chunk %u already received, skipping", chunk_index);
        return true;
    }

    // CRITICAL: esp_ota_write() appends sequentially and cannot seek
    // We MUST receive chunks in order for now
    // TODO: Implement buffering or use esp_partition_write() for out-of-order support
    if (chunk_index != session.chunks_received) {
        ESP_LOGE(TAG, "Out-of-order chunk %u (expected %u) - not supported yet",
                 chunk_index, session.chunks_received);
        ESP_LOGE(TAG, "Chunks must be received sequentially with current implementation");
        return false;
    }

    // Validate chunk size
    if (len != session.chunk_size && chunk_index != session.total_chunks - 1) {
        ESP_LOGE(TAG, "Invalid chunk size %u (expected %u)", len, session.chunk_size);
        return false;
    }

    // Write chunk to OTA partition (sequential append)
    esp_err_t err = esp_ota_write(ota_handle, data, len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_write failed for chunk %u: %d", chunk_index, err);
        return false;
    }

    // Mark chunk as received
    markChunkReceived(chunk_index);
    session.chunks_received++;
    session.last_update_ms = millis();

    ESP_LOGD(TAG, "Chunk %u/%u received (%.1f%%)",
             session.chunks_received, session.total_chunks,
             100.0f * session.chunks_received / session.total_chunks);

    // Periodic checkpoint every 100 chunks
    if (session.chunks_received % 100 == 0) {
        saveSessionCheckpoint();
    }

    return true;
}

bool OTAManager::finalizeUpdate(const uint8_t* signature, size_t sig_len) {
    if (session.state != OTA_DOWNLOADING) {
        ESP_LOGE(TAG, "Cannot finalize - not in downloading state");
        return false;
    }

    // Verify all chunks received
    if (session.chunks_received != session.total_chunks) {
        ESP_LOGE(TAG, "Incomplete download: %u/%u chunks",
                 session.chunks_received, session.total_chunks);
        return false;
    }

    ESP_LOGI(TAG, "All chunks received, finalizing OTA...");

    session.state = OTA_VERIFYING;

    // End OTA write
    esp_err_t err = esp_ota_end(ota_handle);
    ota_begun = false;

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed: %d", err);
        session.state = OTA_FAILED;
        return false;
    }

    // CRITICAL: Verify SHA256 hash BEFORE signature (cheaper check first)
    if (!verifyFinalHash()) {
        ESP_LOGE(TAG, "Firmware hash verification failed");
        session.state = OTA_FAILED;
        return false;
    }

    ESP_LOGI(TAG, "Firmware hash verified");

    // CRITICAL: Verify RSA-2048 signature (required for security)
    if (!signature || sig_len == 0) {
        ESP_LOGE(TAG, "No signature provided - RSA verification required");
        session.state = OTA_FAILED;
        return false;
    }

    // Verify signature using streaming (no large malloc - uses only 4KB RAM)
    if (!verifyFirmwareSignature(signature, sig_len)) {
        ESP_LOGE(TAG, "RSA signature verification FAILED - rejecting update");
        session.state = OTA_FAILED;
        return false;
    }

    ESP_LOGI(TAG, "RSA signature verified - firmware authenticated");

    // Set boot partition
    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %d", err);
        session.state = OTA_FAILED;
        return false;
    }

    session.state = OTA_COMPLETE;
    clearSessionFromNVS();

    ESP_LOGI(TAG, "OTA complete! Reboot to apply update.");

    return true;
}

void OTAManager::abortUpdate() {
    ESP_LOGW(TAG, "Aborting OTA update");

    if (ota_begun) {
        esp_ota_abort(ota_handle);
        ota_begun = false;
    }

    if (session.chunk_bitmap) {
        free(session.chunk_bitmap);
        session.chunk_bitmap = nullptr;
    }

    resetSession();
    clearSessionFromNVS();
}

uint8_t OTAManager::getProgress() const {
    if (session.total_chunks == 0) return 0;
    return (session.chunks_received * 100) / session.total_chunks;
}

bool OTAManager::isUpdateInProgress() const {
    return (session.state == OTA_DOWNLOADING ||
            session.state == OTA_VERIFYING ||
            session.state == OTA_APPLYING);
}

bool OTAManager::performDiagnostics() {
    ESP_LOGI(TAG, "Running OTA health diagnostics...");

    // 1. Check free heap
    uint32_t free_heap = esp_get_free_heap_size();
    if (free_heap < 50000) {
        ESP_LOGE(TAG, "DIAG FAIL: Low heap: %u bytes", free_heap);
        return false;
    }
    ESP_LOGI(TAG, "DIAG PASS: Free heap: %u bytes", free_heap);

    // 2. Test NVS access
    Preferences test_prefs;
    if (!test_prefs.begin("ota_diag", false)) {
        ESP_LOGE(TAG, "DIAG FAIL: NVS access failed");
        return false;
    }
    test_prefs.putUInt("test", millis());
    uint32_t test_val = test_prefs.getUInt("test", 0);
    test_prefs.end();

    if (test_val == 0) {
        ESP_LOGE(TAG, "DIAG FAIL: NVS read/write failed");
        return false;
    }
    ESP_LOGI(TAG, "DIAG PASS: NVS access OK");

    // 3. Verify partition table
    const esp_partition_t* running = esp_ota_get_running_partition();
    if (!running) {
        ESP_LOGE(TAG, "DIAG FAIL: Cannot get running partition");
        return false;
    }
    ESP_LOGI(TAG, "DIAG PASS: Running partition: %s", running->label);

    // Note: Radio initialization test is done in main.cpp setup()
    // We don't want to re-initialize the radio here

    ESP_LOGI(TAG, "All diagnostics passed");
    return true;
}

bool OTAManager::verifyFirmwareSignature(const uint8_t* signature, size_t sig_len) {
    // CRITICAL: Stream firmware from flash to compute hash (no large malloc)
    // Prevents OOM on devices with limited RAM (ESP32-S3 has ~320KB vs ~900KB firmware)

    // Calculate SHA256 hash by streaming firmware from partition
    uint8_t hash[32];
    mbedtls_sha256_context ctx;

    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);  // 0 = SHA256 (not SHA224)

    // Stream in 4KB chunks (only 4KB RAM needed instead of 900KB)
    const size_t chunk_size = 4096;
    uint8_t* buffer = (uint8_t*)malloc(chunk_size);
    if (!buffer) {
        ESP_LOGE(TAG, "Failed to allocate hash buffer");
        return false;
    }

    size_t total_read = 0;
    while (total_read < session.firmware_size) {
        size_t remaining = session.firmware_size - total_read;
        size_t read_size = min(chunk_size, remaining);

        esp_err_t err = esp_partition_read(update_partition, total_read, buffer, read_size);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Partition read failed at offset %u: %d", total_read, err);
            free(buffer);
            mbedtls_sha256_free(&ctx);
            return false;
        }

        mbedtls_sha256_update(&ctx, buffer, read_size);
        total_read += read_size;
    }

    mbedtls_sha256_finish(&ctx, hash);
    mbedtls_sha256_free(&ctx);
    free(buffer);

    // Parse public key
    mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);

    int ret = mbedtls_pk_parse_public_key(&pk,
        (const unsigned char*)ota_public_key_pem,
        strlen(ota_public_key_pem) + 1);

    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to parse public key: -0x%04x", -ret);
        mbedtls_pk_free(&pk);
        return false;
    }

    // Verify signature against computed hash
    ret = mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256,
        hash, sizeof(hash), signature, sig_len);

    mbedtls_pk_free(&pk);

    if (ret != 0) {
        ESP_LOGE(TAG, "Signature verification failed: -0x%04x", -ret);
        return false;
    }

    ESP_LOGI(TAG, "Signature verification passed");
    return true;
}

bool OTAManager::verifyFinalHash() {
    // Calculate SHA256 of OTA partition
    uint8_t calculated_hash[32];
    mbedtls_sha256_context ctx;

    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);  // 0 = SHA256 (not SHA224)

    // Read partition in chunks and hash
    const size_t chunk_size = 4096;
    uint8_t* buffer = (uint8_t*)malloc(chunk_size);
    if (!buffer) {
        ESP_LOGE(TAG, "Failed to allocate hash buffer");
        mbedtls_sha256_free(&ctx);
        return false;
    }

    // CRITICAL FIX: Read exactly firmware_size bytes, no more
    // Bug was: checked total_read AFTER hashing the chunk, so could hash garbage past firmware_size
    size_t total_read = 0;
    while (total_read < session.firmware_size) {
        size_t remaining = session.firmware_size - total_read;
        size_t read_size = min(chunk_size, remaining);  // Never exceed firmware_size

        esp_err_t err = esp_partition_read(update_partition, total_read, buffer, read_size);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Partition read failed at offset %u: %d", total_read, err);
            free(buffer);
            mbedtls_sha256_free(&ctx);
            return false;
        }

        mbedtls_sha256_update(&ctx, buffer, read_size);
        total_read += read_size;
    }

    mbedtls_sha256_finish(&ctx, calculated_hash);
    mbedtls_sha256_free(&ctx);
    free(buffer);

    // Compare hashes
    if (memcmp(calculated_hash, session.firmware_sha256, 32) != 0) {
        ESP_LOGE(TAG, "Hash mismatch!");
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, calculated_hash, 32, ESP_LOG_ERROR);
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, session.firmware_sha256, 32, ESP_LOG_ERROR);
        return false;
    }

    return true;
}

void OTAManager::resetSession() {
    if (session.chunk_bitmap) {
        free(session.chunk_bitmap);
    }
    memset(&session, 0, sizeof(session));
    session.state = OTA_IDLE;
}

void OTAManager::markChunkReceived(uint16_t chunk_index) {
    if (chunk_index >= session.total_chunks) return;

    size_t byte_index = chunk_index / 8;
    uint8_t bit_mask = 1 << (chunk_index % 8);

    session.chunk_bitmap[byte_index] |= bit_mask;
}

bool OTAManager::isChunkReceived(uint16_t chunk_index) const {
    if (chunk_index >= session.total_chunks) return false;

    size_t byte_index = chunk_index / 8;
    uint8_t bit_mask = 1 << (chunk_index % 8);

    return (session.chunk_bitmap[byte_index] & bit_mask) != 0;
}

bool OTAManager::loadSessionFromNVS() {
    if (!prefs.begin("ota_session", true)) {
        return false;
    }

    uint32_t saved_session_id = prefs.getUInt("session_id", 0);
    if (saved_session_id == 0) {
        prefs.end();
        return false;
    }

    session.session_id = saved_session_id;
    session.total_chunks = prefs.getUShort("total_chunks", 0);
    session.chunks_received = prefs.getUShort("chunks_rx", 0);
    session.state = OTA_DOWNLOADING;
    session.transport = (OTATransport)prefs.getUChar("transport", OTA_TRANSPORT_LORA);

    prefs.getBytes("fw_sha256", session.firmware_sha256, 32);

    // CRITICAL: Restore firmware_size and chunk_size (required for hash verification and validation)
    session.firmware_size = prefs.getUInt("fw_size", 0);
    session.chunk_size = prefs.getUShort("chunk_size", 0);

    // Reallocate bitmap
    session.bitmap_size = (session.total_chunks + 7) / 8;
    session.chunk_bitmap = (uint8_t*)malloc(session.bitmap_size);
    if (session.chunk_bitmap) {
        prefs.getBytes("bitmap", session.chunk_bitmap, session.bitmap_size);
    }

    prefs.end();

    // NOTE: esp_ota_handle_t cannot be serialized/restored
    // Resume only works for sequential chunks (Phase 1 limitation)
    // For true out-of-order resume, need to switch to esp_partition_write()
    ESP_LOGW(TAG, "Session resumed - expecting sequential chunk %u next", session.chunks_received);

    return true;
}

bool OTAManager::saveSessionToNVS() {
    if (!prefs.begin("ota_session", false)) {
        return false;
    }

    prefs.putUInt("session_id", session.session_id);
    prefs.putUShort("total_chunks", session.total_chunks);
    prefs.putUShort("chunks_rx", session.chunks_received);
    prefs.putUChar("transport", (uint8_t)session.transport);
    prefs.putBytes("fw_sha256", session.firmware_sha256, 32);

    // CRITICAL: Save firmware_size and chunk_size for hash verification and resume
    prefs.putUInt("fw_size", session.firmware_size);
    prefs.putUShort("chunk_size", session.chunk_size);

    if (session.chunk_bitmap) {
        prefs.putBytes("bitmap", session.chunk_bitmap, session.bitmap_size);
    }

    prefs.end();
    return true;
}

void OTAManager::clearSessionFromNVS() {
    if (prefs.begin("ota_session", false)) {
        prefs.clear();
        prefs.end();
    }
}

void OTAManager::saveSessionCheckpoint() {
    if (session.state == OTA_DOWNLOADING) {
        saveSessionToNVS();
        ESP_LOGD(TAG, "Session checkpoint saved (%u/%u chunks)",
                 session.chunks_received, session.total_chunks);
    }
}

bool OTAManager::resumeSession() {
    return loadSessionFromNVS();
}

void OTAManager::updateAirtimeBudget() {
    uint32_t now = millis();

    // Reset window every hour (3600 seconds)
    if (now - lora_window_start_ms >= 3600000) {
        lora_airtime_used_ms = 0;
        lora_window_start_ms = now;
        ESP_LOGD(TAG, "LoRa airtime budget reset");
    }
}

bool OTAManager::canSendLoRaChunk() {
    // 1% duty cycle = 36 seconds per hour
    constexpr uint32_t MAX_AIRTIME_MS = 36000;

    return lora_airtime_used_ms < MAX_AIRTIME_MS;
}

uint32_t OTAManager::getLoRaDelayMs() const {
    // 43-second inter-chunk delay for duty cycle compliance
    // See OTA-UPDATE-SPEC.md lines 217-274
    return 43000;
}

} // namespace lorapnp
