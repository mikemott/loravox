# OTA Tools

Utilities for generating and managing OTA firmware updates.

---

## generate_delta.py

Block-level delta firmware generation for efficient OTA updates over LoRa.

### Features

- **Block-level comparison** - Compares firmware at 4KB granularity
- **CRC16 verification** - Per-block checksums for integrity
- **Delta optimization** - Only transfers changed blocks (50-95% savings)
- **LoRa timing** - Estimates transfer time with duty cycle
- **Verification mode** - Validates delta file integrity

### Usage

#### Generate Delta

```bash
python3 tools/generate_delta.py old_firmware.bin new_firmware.bin delta.bin
```

**Example Output:**
```
Old firmware: 892,733 bytes
New firmware: 895,421 bytes
Old SHA256: abc123...
New SHA256: def456...

Delta Statistics:
  Total blocks: 219
  Changed blocks: 12 (5.5%)
  Delta size: 49,268 bytes
  Compression: 5.5% of full firmware
  Savings: 846,153 bytes (94.5%)

LoRa Transfer Estimate:
  Chunks needed: 247
  Transfer time: 17.7 minutes

✓ Delta saved to: delta.bin
```

#### Verify Delta

```bash
python3 tools/generate_delta.py --verify delta.bin
```

**Example Output:**
```
Delta Header:
  Total blocks: 219
  Changed blocks: 12
  Base hash: abc123...
  New hash: def456...
  ✓ All 12 blocks verified
```

### Delta File Format

```
Header (68 bytes):
  - total_blocks (2 bytes, little-endian)
  - changed_blocks (2 bytes, little-endian)
  - base_fw_sha256 (32 bytes)
  - new_fw_sha256 (32 bytes)

Changed Blocks (4100 bytes each):
  - block_index (2 bytes, little-endian)
  - data (4096 bytes, 0xFF padded)
  - crc16 (2 bytes, little-endian)
```

### Transfer Time Calculation

**LoRa Constraints:**
- 1% duty cycle = 36 seconds airtime per hour
- Chunk size: ~200 bytes (LoRa payload limit)
- Inter-chunk delay: 43 seconds (duty cycle compliance)

**Example:**
- Delta size: 49,268 bytes
- Chunks needed: 247 (49,268 / 200)
- Transfer time: 247 × 43s = 10,621s = **17.7 minutes**

**WiFi/BLE:** Full transfer in seconds, no chunking needed.

### Typical Use Cases

#### Minor Bug Fix (5-10% changed)
```
Old: v1.0.0 (892 KB)
New: v1.0.1 (895 KB)
Delta: ~50 KB (5.6%)
LoRa: ~18 minutes
```

#### Feature Addition (20-30% changed)
```
Old: v1.0.0 (892 KB)
New: v1.1.0 (920 KB)
Delta: ~200 KB (22%)
LoRa: ~72 minutes
WiFi: <5 seconds
```

**Recommendation:** Use LoRa for deltas < 100 KB, WiFi/BLE for larger updates.

### Integration with Build System

#### PlatformIO Post-Build Script

Add to `platformio.ini`:

```ini
[env:lilygo-t3s3]
extra_scripts = post:tools/post_build.py
```

Create `tools/post_build.py`:

```python
Import("env")
import subprocess
import os

def generate_delta(source, target, env):
    firmware_new = str(target[0])
    firmware_old = "releases/firmware-v1.0.0.bin"
    delta_out = firmware_new.replace(".bin", "-delta.bin")

    if os.path.exists(firmware_old):
        subprocess.run([
            "python3", "tools/generate_delta.py",
            firmware_old, firmware_new, delta_out
        ])
        print(f"Delta generated: {delta_out}")

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", generate_delta)
```

#### GitHub Actions

```yaml
- name: Generate Delta
  run: |
    wget https://releases.example.com/firmware-v1.0.0.bin -O old.bin
    python3 tools/generate_delta.py old.bin .pio/build/lilygo-t3s3/firmware.bin delta.bin

- name: Upload Delta
  uses: actions/upload-artifact@v3
  with:
    name: firmware-delta
    path: delta.bin
```

### Algorithm Details

**Block Comparison:**
1. Divide firmware into 4KB blocks
2. Compare each block byte-by-byte
3. If different, include in delta
4. Pad last block with 0xFF

**CRC16:**
- Algorithm: CRC16-CCITT
- Polynomial: 0xA001
- Per-block verification on device

**SHA256:**
- Full firmware hash for base/new
- Validates delta compatibility
- Verifies final result

### Limitations

1. **No compression** - Blocks stored as-is (future: zlib)
2. **Fixed block size** - 4KB (optimal for ESP32 flash)
3. **No encryption** - Handled separately by RSA signatures
4. **Single delta** - No multi-version delta chains

### Future Enhancements

- [ ] Delta compression (zlib)
- [ ] Bsdiff algorithm option (better compression)
- [ ] Multi-version delta chains
- [ ] Delta preview/simulation mode
- [ ] Binary diff visualization

---

## Future Tools

### sign_firmware.py
RSA-2048 signing for production builds.

### verify_firmware.py
Offline signature and hash verification.

### ota_server.py
Simple HTTP server for OTA distribution.

---

## Requirements

- Python 3.7+
- No external dependencies (uses stdlib only)

---

## Related Documentation

- `../OTA-UPDATE-SPEC.md` - Complete OTA specification
- `../lib/LoRaPNP/src/OTAManager.h` - Device-side implementation
