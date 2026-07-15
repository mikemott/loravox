#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <Arduino.h>
#include <esp_ota_ops.h>
#include <mbedtls/pk.h>
#include <mbedtls/sha256.h>
#include <Preferences.h>
#include "Packet.h"
#include "Types.h"

namespace lorapnp {

constexpr uint32_t DELTA_BLOCK_SIZE = 4096;  // 4 KB blocks
constexpr uint32_t MAX_FIRMWARE_SIZE = 1572864;  // 1.5 MB
constexpr uint16_t MAX_BLOCKS = MAX_FIRMWARE_SIZE / DELTA_BLOCK_SIZE;  // 384 blocks

enum OTAState {
    OTA_IDLE,
    OTA_ANNOUNCED,       // Update available
    OTA_DOWNLOADING,     // Receiving chunks
    OTA_VERIFYING,       // Signature verification
    OTA_APPLYING,        // Writing to flash
    OTA_COMPLETE,        // Ready to reboot
    OTA_FAILED
};

enum OTATransport {
    OTA_TRANSPORT_WIFI = 0,
    OTA_TRANSPORT_BLE = 1,
    OTA_TRANSPORT_LORA = 2
};

struct DeltaBlockHeader {
    uint16_t total_blocks;        // Total blocks in firmware
    uint16_t changed_blocks;      // Number of blocks in this delta
    uint8_t base_fw_sha256[32];   // SHA256 of base firmware
    uint8_t new_fw_sha256[32];    // SHA256 of resulting firmware
};

struct OTASessionState {
    uint32_t session_id;          // 32-bit session nonce
    uint16_t total_chunks;        // Total chunks to receive
    uint16_t chunks_received;     // Chunks received so far
    uint8_t* chunk_bitmap;        // Dynamic allocation based on total_chunks
    size_t bitmap_size;           // Bitmap size in bytes
    uint8_t firmware_sha256[32];  // Expected final hash
    uint32_t last_update_ms;      // For timeout detection
    OTAState state;
    OTATransport transport;       // Transport being used
    uint32_t firmware_size;       // Total firmware size in bytes
    uint16_t chunk_size;          // Size of each chunk
};

class OTAManager {
public:
    OTAManager();
    ~OTAManager();

    // Lifecycle
    bool begin();
    void end();
    void update();

    // OTA control
    bool startUpdate(uint32_t session_id, uint16_t total_chunks,
                     const uint8_t* firmware_sha256, OTATransport transport,
                     uint32_t firmware_size, uint16_t chunk_size);
    bool processChunk(uint16_t chunk_index, const uint8_t* data, size_t len);
    bool finalizeUpdate(const uint8_t* signature, size_t sig_len);  // RSA signature required
    void abortUpdate();

    // State queries
    OTAState getState() const { return session.state; }
    uint8_t getProgress() const;  // 0-100
    uint32_t getSessionId() const { return session.session_id; }
    bool isUpdateInProgress() const;

    // Health check for rollback protection
    bool performDiagnostics();

    // Signature verification (RSA-2048) - streaming implementation
    bool verifyFirmwareSignature(const uint8_t* signature, size_t sig_len);

    // Session resume (after power loss)
    bool resumeSession();
    void saveSessionCheckpoint();

    // Duty cycle tracking for LoRa
    void updateAirtimeBudget();
    bool canSendLoRaChunk();
    uint32_t getLoRaDelayMs() const;

private:
    // Internal state management
    bool validateHeader(const DeltaBlockHeader* header, size_t delta_len);
    bool applyBlockDelta(const uint8_t* delta_data, size_t delta_len);
    bool verifyFinalHash();
    void resetSession();
    void markChunkReceived(uint16_t chunk_index);
    bool isChunkReceived(uint16_t chunk_index) const;

    // NVS persistence
    bool loadSessionFromNVS();
    bool saveSessionToNVS();
    void clearSessionFromNVS();

    // Airtime tracking (LoRa duty cycle)
    uint32_t lora_airtime_used_ms;
    uint32_t lora_window_start_ms;
    uint32_t last_chunk_sent_ms;

    // OTA session state
    OTASessionState session;
    const esp_partition_t* update_partition;

    // Preferences for NVS
    Preferences prefs;

    // Public key — defined in OTAManager.cpp from OTAPublicKey.h (shared with GatewayOTA).
    static const char* ota_public_key_pem;
};

} // namespace lorapnp

#endif // OTA_MANAGER_H
