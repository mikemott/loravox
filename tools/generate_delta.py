#!/usr/bin/env python3
"""
Block-level delta firmware generation for LoRa OTA updates.

Compares old and new firmware binaries at 4KB block granularity
and generates a delta file containing only changed blocks.

Usage:
    python3 generate_delta.py old.bin new.bin delta.bin

Output format:
    - Header (68 bytes):
        - total_blocks (2 bytes)
        - changed_blocks (2 bytes)
        - base_fw_sha256 (32 bytes)
        - new_fw_sha256 (32 bytes)
    - Changed blocks (4100 bytes each):
        - block_index (2 bytes)
        - data (4096 bytes)
        - crc16 (2 bytes)
"""

import sys
import hashlib
import struct
import zlib

BLOCK_SIZE = 4096  # 4 KB blocks


def crc16(data):
    """Calculate CRC16 checksum."""
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x0001:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc & 0xFFFF


def sha256_file(filename):
    """Calculate SHA256 hash of a file."""
    h = hashlib.sha256()
    with open(filename, 'rb') as f:
        while True:
            chunk = f.read(65536)
            if not chunk:
                break
            h.update(chunk)
    return h.digest()


def generate_delta(old_fw_path, new_fw_path, delta_path):
    """Generate block-level delta between two firmware binaries."""

    # Read firmware files
    with open(old_fw_path, 'rb') as f:
        old_data = f.read()

    with open(new_fw_path, 'rb') as f:
        new_data = f.read()

    # Calculate hashes
    old_hash = sha256_file(old_fw_path)
    new_hash = sha256_file(new_fw_path)

    print(f"Old firmware: {len(old_data):,} bytes")
    print(f"New firmware: {len(new_data):,} bytes")
    print(f"Old SHA256: {old_hash.hex()}")
    print(f"New SHA256: {new_hash.hex()}")

    # Calculate total blocks based on new firmware size
    total_blocks = (len(new_data) + BLOCK_SIZE - 1) // BLOCK_SIZE

    # Pad old firmware if needed
    if len(old_data) < len(new_data):
        old_data += b'\xff' * (len(new_data) - len(old_data))

    # Find changed blocks
    changed_blocks = []

    for i in range(total_blocks):
        start = i * BLOCK_SIZE
        end = min(start + BLOCK_SIZE, len(new_data))

        old_block = old_data[start:end] if start < len(old_data) else b'\xff' * (end - start)
        new_block = new_data[start:end]

        # Pad new block to BLOCK_SIZE
        if len(new_block) < BLOCK_SIZE:
            new_block += b'\xff' * (BLOCK_SIZE - len(new_block))

        # Compare blocks
        if old_block != new_block:
            block_crc = crc16(new_block)
            changed_blocks.append((i, new_block, block_crc))

    # Write delta file
    with open(delta_path, 'wb') as f:
        # Header
        f.write(struct.pack('<HH', total_blocks, len(changed_blocks)))
        f.write(old_hash)
        f.write(new_hash)

        # Changed blocks
        for block_idx, block_data, block_crc in changed_blocks:
            f.write(struct.pack('<H', block_idx))
            f.write(block_data)
            f.write(struct.pack('<H', block_crc))

    # Statistics
    delta_size = 68 + (len(changed_blocks) * 4100)
    compression_ratio = (delta_size / len(new_data)) * 100 if len(new_data) > 0 else 0

    print(f"\nDelta Statistics:")
    print(f"  Total blocks: {total_blocks}")
    print(f"  Changed blocks: {len(changed_blocks)} ({100 * len(changed_blocks) / total_blocks:.1f}%)")
    print(f"  Delta size: {delta_size:,} bytes")
    print(f"  Compression: {compression_ratio:.1f}% of full firmware")
    print(f"  Savings: {len(new_data) - delta_size:,} bytes ({100 - compression_ratio:.1f}%)")

    # Estimate LoRa transfer time
    # Chunk size ~200 bytes, 43-second inter-chunk delay
    chunks_needed = (delta_size + 199) // 200
    transfer_time_s = chunks_needed * 43
    transfer_time_min = transfer_time_s / 60

    print(f"\nLoRa Transfer Estimate:")
    print(f"  Chunks needed: {chunks_needed}")
    print(f"  Transfer time: {transfer_time_min:.1f} minutes")

    if transfer_time_min > 30:
        print(f"  ⚠️  WARNING: Transfer time exceeds 30 minutes")
        print(f"  Consider reducing changes or using WiFi/BLE transport")

    print(f"\n✓ Delta saved to: {delta_path}")


def verify_delta(delta_path):
    """Verify delta file integrity."""
    with open(delta_path, 'rb') as f:
        # Read header
        total_blocks, changed_blocks = struct.unpack('<HH', f.read(4))
        base_hash = f.read(32)
        new_hash = f.read(32)

        print(f"Delta Header:")
        print(f"  Total blocks: {total_blocks}")
        print(f"  Changed blocks: {changed_blocks}")
        print(f"  Base hash: {base_hash.hex()}")
        print(f"  New hash: {new_hash.hex()}")

        # Verify all blocks are present
        for i in range(changed_blocks):
            block_idx = struct.unpack('<H', f.read(2))[0]
            block_data = f.read(4096)
            block_crc = struct.unpack('<H', f.read(2))[0]

            # Verify CRC
            calculated_crc = crc16(block_data)
            if calculated_crc != block_crc:
                print(f"  ✗ Block {i}: CRC mismatch (expected {block_crc:04x}, got {calculated_crc:04x})")
                return False

        print(f"  ✓ All {changed_blocks} blocks verified")
        return True


def main():
    if len(sys.argv) < 3:
        print("Usage: generate_delta.py old.bin new.bin [delta.bin]")
        print()
        print("Options:")
        print("  --verify delta.bin    Verify delta file integrity")
        sys.exit(1)

    if sys.argv[1] == '--verify':
        if len(sys.argv) < 3:
            print("Error: Missing delta file path")
            sys.exit(1)
        success = verify_delta(sys.argv[2])
        sys.exit(0 if success else 1)

    old_fw = sys.argv[1]
    new_fw = sys.argv[2]
    delta_fw = sys.argv[3] if len(sys.argv) > 3 else 'delta.bin'

    try:
        generate_delta(old_fw, new_fw, delta_fw)
    except FileNotFoundError as e:
        print(f"Error: {e}")
        sys.exit(1)
    except Exception as e:
        print(f"Error generating delta: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == '__main__':
    main()
