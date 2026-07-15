#pragma once

// RSA-2048 public key for firmware signature verification (single source of truth).
//
// Used by both:
//   - OTAManager (relay/LoRa OTA receiver on nodes)
//   - GatewayOTA (gateway WiFi self-update)
//
// Private key location: tools/keys/firmware_signing_key.pem (KEEP SECRET - NOT IN GIT)
// To sign firmware:     python3 tools/sign_firmware.py firmware.bin
// Signed artifact:      [firmware bytes][256-byte RSA-2048 PKCS#1 v1.5 signature over SHA-256]
// Generated: 2026-07-06
//
// `static` gives each translation unit its own copy (internal linkage) — there is
// only ONE definition of the key text in the source tree, which is the point:
// the two consumers can never drift to different keys.
static const char OTA_PUBLIC_KEY_PEM[] =
    "-----BEGIN PUBLIC KEY-----\n"
    "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA40/IitxjkQzS4p8VhN/c\n"
    "JCJx31yc/FvxfkI34z4jP9e1CcIRWiDeMWL+QwOg8rhmovRtTsHibL1dweJyeJws\n"
    "kMOae0h2fLrB2FA9UXhGQkQPHDO6/HVhWCEMLIlXkfcfrNWtPozR4H5n3TedeO0O\n"
    "O/crmuAQLLcsdCPbXOc91mG/r00jVI0naSE1w6pdjRCHfLNRk5emMZykqCCL2avG\n"
    "EtSg7aa15TSxa4w2+PPYGlkHx8vPKTCF1hD6TafaU7FyuSGBBYGNZylcCv8tgT5W\n"
    "VQqe1C8J/mELwPW7horcyuirJRYiVXX80GXY3Y0l0nfcw7Del/udGOF5Ad6U/XqH\n"
    "cQIDAQAB\n"
    "-----END PUBLIC KEY-----\n";
