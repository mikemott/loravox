#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include <mbedtls/sha256.h>

namespace lorapnp {

// Gateway WiFi self-update.
//
// Serves a signed-firmware upload endpoint on the gateway's existing WebServer:
//   GET  /update   -> minimal HTML upload form
//   POST /update   -> multipart firmware upload (field name irrelevant)
//
// The uploaded file is the signed artifact produced by tools/sign_firmware.py:
//   [firmware bytes][256-byte RSA-2048 PKCS#1 v1.5 signature over SHA-256]
//
// Design: the upload is streamed straight into the ESP32 Update partition while a
// rolling 256-byte tail (the trailing signature) is withheld and the SHA-256 is
// computed over the firmware bytes only. The RSA signature is verified BEFORE the
// boot partition is committed. RAM cost is flat (~256 B tail + hash ctx), so this
// works regardless of image size. Nothing here touches the LoRa packet protocol.
//
// This covers gateway self-update only. Pushing firmware to nodes/remote sensors
// (over WiFi/BLE/LoRa via the network-selection layer) is separate, later work.
class GatewayOTA {
public:
    // Register routes on an already-constructed WebServer (call before or after begin()).
    void begin(WebServer* server);

    // Call from loop(): performs the deferred reboot after a verified update.
    void loop();

    bool inProgress() const { return active_; }

private:
    void handleForm();      // GET  /update
    void handleFinish();    // POST /update (response after upload completes)
    void handleUpload();    // POST /update (streaming upload callback)

    void reset();
    void fail(const char* msg);
    void emit(const uint8_t* data, size_t len);         // Update.write + hash
    void feed(const uint8_t* buf, size_t len);          // tail-strip window
    bool verifySignature(const uint8_t* hash, const uint8_t* sig);

    WebServer* server_ = nullptr;

    bool active_   = false;   // an upload is being processed
    bool hashing_  = false;   // sha ctx initialised (needs free)
    bool failed_   = false;
    bool verified_ = false;
    const char* err_ = nullptr;

    size_t   fw_bytes_ = 0;   // firmware bytes written so far (excludes signature)
    uint8_t  tail_[256];      // rolling window holding the trailing signature
    size_t   tail_len_ = 0;
    mbedtls_sha256_context sha_;

    uint32_t reboot_at_ms_ = 0;  // 0 = no reboot pending
};

} // namespace lorapnp
