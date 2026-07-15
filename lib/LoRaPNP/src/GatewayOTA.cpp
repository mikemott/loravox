#include "GatewayOTA.h"
#include "OTAPublicKey.h"
#include <Update.h>
#include <mbedtls/pk.h>
#include <string.h>

namespace lorapnp {

static constexpr size_t SIG_LEN = 256;  // RSA-2048 signature length

static const char UPLOAD_FORM[] PROGMEM =
    "<!doctype html><meta name=viewport content='width=device-width,initial-scale=1'>"
    "<title>Gateway Firmware Update</title>"
    "<style>body{font-family:system-ui;background:#111;color:#eee;max-width:32rem;margin:2rem auto;padding:0 1rem}"
    "h1{font-size:1.2rem}button{padding:.6rem 1rem;font-size:1rem}#s{margin-top:1rem}</style>"
    "<h1>Gateway Firmware Update</h1>"
    "<p>Upload a signed firmware image (<code>firmware.bin.signed</code>).</p>"
    "<form id=f><input type=file name=fw required> <button>Upload</button></form>"
    "<div id=s></div>"
    "<script>f.onsubmit=async e=>{e.preventDefault();s.textContent='Uploading & verifying...';"
    "let fd=new FormData(f);try{let r=await fetch('/update',{method:'POST',body:fd});"
    "let j=await r.json();s.textContent=(r.ok?'\\u2713 ':'\\u2717 ')+j.message;}"
    "catch(err){s.textContent='Upload failed: '+err;}};</script>";

void GatewayOTA::begin(WebServer* server) {
    server_ = server;
    server_->on("/update", HTTP_GET,  [this]() { handleForm(); });
    server_->on("/update", HTTP_POST, [this]() { handleFinish(); },
                                      [this]() { handleUpload(); });
}

void GatewayOTA::loop() {
    if (reboot_at_ms_ != 0 && (int32_t)(millis() - reboot_at_ms_) >= 0) {
        ESP.restart();
    }
}

void GatewayOTA::handleForm() {
    server_->send_P(200, "text/html", UPLOAD_FORM);
}

void GatewayOTA::freeHash() {
    if (hashing_) {
        mbedtls_sha256_free(&sha_);
        hashing_ = false;
    }
}

void GatewayOTA::reset() {
    freeHash();
    active_ = false;
    failed_ = false;
    verified_ = false;
    err_ = nullptr;
    fw_bytes_ = 0;
    tail_len_ = 0;
}

void GatewayOTA::fail(const char* msg) {
    if (!failed_) {
        failed_ = true;
        err_ = msg;
    }
    freeHash();   // release the SHA context on every terminal error path
}

void GatewayOTA::emit(const uint8_t* data, size_t len) {
    if (failed_ || len == 0) return;
    if (Update.write((uint8_t*)data, len) != len) {
        fail("flash write failed");
        return;
    }
    mbedtls_sha256_update(&sha_, data, len);
    fw_bytes_ += len;
}

// Slide a 256-byte window across the stream: everything except the trailing 256
// bytes is firmware (emitted + hashed); the final 256 bytes are the signature.
void GatewayOTA::feed(const uint8_t* buf, size_t len) {
    if (failed_ || len == 0) return;

    size_t total = tail_len_ + len;
    if (total <= SIG_LEN) {                 // still all inside the window
        memcpy(tail_ + tail_len_, buf, len);
        tail_len_ = total;
        return;
    }

    size_t flush = total - SIG_LEN;         // bytes now confirmed as firmware
    size_t from_tail = flush < tail_len_ ? flush : tail_len_;
    if (from_tail) emit(tail_, from_tail);
    size_t from_buf = flush - from_tail;
    if (from_buf)  emit(buf, from_buf);

    // Rebuild the window = last SIG_LEN bytes of (old tail ++ buf).
    size_t rem_tail = tail_len_ - from_tail;
    uint8_t next[SIG_LEN];
    memcpy(next, tail_ + from_tail, rem_tail);
    memcpy(next + rem_tail, buf + from_buf, len - from_buf);  // rem_tail + (len-from_buf) == SIG_LEN
    memcpy(tail_, next, SIG_LEN);
    tail_len_ = SIG_LEN;
}

bool GatewayOTA::verifySignature(const uint8_t* hash, const uint8_t* sig) {
    mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);
    bool ok = false;
    if (mbedtls_pk_parse_public_key(&pk, (const unsigned char*)OTA_PUBLIC_KEY_PEM,
                                    strlen(OTA_PUBLIC_KEY_PEM) + 1) == 0) {
        ok = mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256, hash, 32, sig, SIG_LEN) == 0;
    }
    mbedtls_pk_free(&pk);
    return ok;
}

void GatewayOTA::handleUpload() {
    HTTPUpload& up = server_->upload();

    switch (up.status) {
    case UPLOAD_FILE_START:
        reset();
        active_ = true;
        mbedtls_sha256_init(&sha_);
        mbedtls_sha256_starts(&sha_, 0);   // 0 => SHA-256
        hashing_ = true;
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            fail("Update.begin failed (no OTA partition?)");
        }
        break;

    case UPLOAD_FILE_WRITE:
        if (active_ && !failed_) feed(up.buf, up.currentSize);
        break;

    case UPLOAD_FILE_END: {
        if (!active_) break;
        if (failed_) { Update.abort(); break; }
        if (tail_len_ != SIG_LEN) { fail("file smaller than signature"); Update.abort(); break; }
        if (fw_bytes_ == 0)       { fail("empty firmware");             Update.abort(); break; }

        uint8_t hash[32];
        mbedtls_sha256_finish(&sha_, hash);
        freeHash();

        if (!verifySignature(hash, tail_)) { fail("signature verification failed"); Update.abort(); break; }
        if (!Update.end(true))             { fail("Update.end failed");             Update.abort(); break; }
        verified_ = true;
        break;
    }

    case UPLOAD_FILE_ABORTED:
        Update.abort();
        fail("upload aborted");
        break;
    }
}

void GatewayOTA::handleFinish() {
    if (verified_) {
        server_->send(200, "application/json",
                      "{\"status\":\"ok\",\"message\":\"Firmware verified — rebooting\"}");
        reboot_at_ms_ = millis() + 1200;   // let the response flush before restart
        if (reboot_at_ms_ == 0) reboot_at_ms_ = 1;  // avoid the sentinel value
    } else {
        String msg = err_ ? err_ : "update failed";
        server_->send(400, "application/json",
                      String("{\"status\":\"error\",\"message\":\"") + msg + "\"}");
    }
    active_ = false;
}

} // namespace lorapnp
