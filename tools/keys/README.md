# OTA Firmware Signing Keys

This directory contains RSA-2048 keys for signing OTA firmware updates.

## Security Model

**Public Key** (`firmware_public_key.pem`):
- Embedded in device firmware (see `lib/LoRaPNP/src/OTAManager.cpp`)
- Used by devices to verify firmware signatures before applying updates
- Safe to commit to repository

**Private Key** (`firmware_signing_key.pem`):
- ⚠️ **KEEP SECRET** - Never commit to git (excluded in .gitignore)
- Used to sign firmware binaries before distribution
- Store securely (password manager, HSM, or secure backup)
- Anyone with this key can create valid firmware updates

## Key Management

### Generated Keys

This repository includes a generated RSA-2048 key pair:
- **Public Key**: Embedded in firmware
- **Private Key**: Located in `tools/keys/firmware_signing_key.pem`
- **Generated**: 2026-07-06

⚠️ **For production deployment**: Generate your own keys and store the private key securely.

### Generate New Keys

To generate a new RSA-2048 key pair:

```bash
# Generate private key (KEEP SECRET)
openssl genrsa -out tools/keys/firmware_signing_key.pem 2048

# Extract public key (embed in firmware)
openssl rsa -in tools/keys/firmware_signing_key.pem -pubout -out tools/keys/firmware_public_key.pem

# View public key
cat tools/keys/firmware_public_key.pem
```

After generating new keys:
1. Update the public key in `lib/LoRaPNP/src/OTAManager.cpp` (lines 14-22)
2. Rebuild and reflash all devices
3. Store private key securely
4. Use `tools/sign_firmware.py` to sign future updates

## Signing Firmware

Use the provided signing script to create signed firmware:

```bash
# Sign firmware binary
python3 tools/sign_firmware.py .pio/build/lilygo-t3s3/firmware.bin

# Output: firmware.bin.signed (original firmware + 256-byte signature)
```

### Signing Workflow

1. **Build firmware**:
   ```bash
   pio run -e lilygo-t3s3
   ```

2. **Sign firmware**:
   ```bash
   python3 tools/sign_firmware.py .pio/build/lilygo-t3s3/firmware.bin -v
   ```
   The `-v` flag verifies the signature after signing.

3. **Upload to OTA server**:
   - Upload `firmware.bin.signed` to your OTA distribution server
   - Announce update with SHA256 hash (displayed by signing script)
   - OTAManager will download, verify signature, and apply update

## Signature Format

Signed firmware format:
```
[Original Firmware Binary (N bytes)]
[RSA-2048 Signature (256 bytes)]
```

The signature is computed over the SHA256 hash of the original firmware.

## Verification Process

When a device receives an OTA update:

1. **Download**: Receive firmware + signature over WiFi/BLE/LoRa
2. **Hash**: Calculate SHA256 of firmware
3. **Verify Signature**: Use embedded public key to verify signature
4. **Compare Hash**: Verify firmware hash matches expected value
5. **Apply**: If valid, write to OTA partition and reboot
6. **Rollback**: If verification fails, reject update

## Key Rotation

To rotate keys (replace compromised or old keys):

1. Generate new key pair (see above)
2. Build new firmware with new public key
3. Sign new firmware with **old** private key
4. Distribute as regular OTA update
5. After all devices update, use new private key for future updates

## Backup and Recovery

**Critical**: Back up your private key securely!

Recommended backup locations:
- Password manager (1Password, Bitwarden, etc.)
- Encrypted USB drive (offline storage)
- Hardware Security Module (HSM) for production
- Secure cloud storage (encrypted)

**DO NOT**:
- Commit private key to git
- Email private key
- Store unencrypted on shared drives
- Hard-code in CI/CD pipelines (use secrets management)

## CI/CD Integration

For automated builds:

1. Store private key in CI/CD secrets (GitHub Actions, GitLab CI, etc.)
2. Inject key at build time:
   ```yaml
   # GitHub Actions example
   - name: Sign firmware
     env:
       SIGNING_KEY: ${{ secrets.FIRMWARE_SIGNING_KEY }}
     run: |
       echo "$SIGNING_KEY" > tools/keys/firmware_signing_key.pem
       python3 tools/sign_firmware.py .pio/build/lilygo-t3s3/firmware.bin
   ```

3. Clean up after build:
   ```yaml
   - name: Cleanup
     run: rm tools/keys/firmware_signing_key.pem
   ```

## Verification

To verify the embedded public key matches your private key:

```bash
# Extract public key from private key
openssl rsa -in tools/keys/firmware_signing_key.pem -pubout

# Compare with firmware_public_key.pem
diff <(openssl rsa -in tools/keys/firmware_signing_key.pem -pubout) tools/keys/firmware_public_key.pem
```

No output means they match.

## Security Best Practices

1. ✅ **Treat private key like a password** - anyone with it can push malicious updates
2. ✅ **Use key rotation** - replace keys periodically (annually recommended)
3. ✅ **Monitor signature failures** - alert on verification failures (potential attack)
4. ✅ **Secure build pipeline** - audit who can sign firmware
5. ✅ **Test rollback** - verify devices reject invalid signatures

## Troubleshooting

### Signature Verification Fails

**Symptom**: OTAManager logs "RSA signature verification FAILED"

**Causes**:
- Wrong public key embedded in firmware
- Firmware signed with wrong private key
- Signature corrupted during transmission
- Firmware binary modified after signing

**Fix**:
1. Verify keys match: `diff <(openssl rsa -in tools/keys/firmware_signing_key.pem -pubout) tools/keys/firmware_public_key.pem`
2. Re-sign firmware: `python3 tools/sign_firmware.py firmware.bin -v`
3. Check transmission integrity (compare SHA256 hashes)

### Private Key Lost

**Symptom**: Cannot sign new firmware updates

**Recovery**:
- If you have backup: Restore from secure backup
- If lost permanently: Generate new keys, rebuild firmware with new public key, distribute via physical access (USB/serial)

---

**For questions or issues, see the main OTA documentation in the repository root.**
