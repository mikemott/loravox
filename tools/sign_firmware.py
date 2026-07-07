#!/usr/bin/env python3
"""
Firmware Signing Tool for LoRaVox OTA Updates

This script signs firmware binaries with RSA-2048 signatures for secure OTA updates.
The signature is verified by OTAManager before applying the update.

Usage:
    python3 tools/sign_firmware.py firmware.bin [-o signed.bin] [-k private_key.pem]

Arguments:
    firmware.bin        Input firmware binary to sign
    -o, --output        Output file (default: firmware.bin.signed)
    -k, --key           Private key file (default: tools/keys/firmware_signing_key.pem)
    -v, --verify        Verify signature after signing

The output file contains:
    [original firmware binary][256-byte RSA-2048 signature]
"""

import sys
import os
import argparse
import hashlib
from pathlib import Path

try:
    from cryptography.hazmat.primitives import hashes, serialization
    from cryptography.hazmat.primitives.asymmetric import padding
    from cryptography.hazmat.backends import default_backend
except ImportError:
    print("ERROR: cryptography library not found")
    print("Install with: pip install cryptography")
    sys.exit(1)


def sign_firmware(firmware_path, private_key_path, output_path, verify=False):
    """Sign firmware with RSA-2048 private key"""

    # Read firmware binary
    print(f"Reading firmware: {firmware_path}")
    with open(firmware_path, 'rb') as f:
        firmware = f.read()

    firmware_size = len(firmware)
    print(f"Firmware size: {firmware_size:,} bytes ({firmware_size / 1024:.1f} KB)")

    # Read private key
    print(f"Loading private key: {private_key_path}")
    with open(private_key_path, 'rb') as f:
        private_key = serialization.load_pem_private_key(
            f.read(),
            password=None,
            backend=default_backend()
        )

    # Calculate SHA256 hash
    print("Calculating SHA256 hash...")
    sha256_hash = hashlib.sha256(firmware).digest()
    print(f"SHA256: {sha256_hash.hex()}")

    # Sign the hash with RSA-2048
    print("Signing firmware with RSA-2048...")
    signature = private_key.sign(
        sha256_hash,
        padding.PKCS1v15(),
        hashes.SHA256()
    )

    sig_size = len(signature)
    print(f"Signature size: {sig_size} bytes")

    if sig_size != 256:
        print(f"WARNING: Expected 256-byte signature for RSA-2048, got {sig_size}")

    # Write signed firmware (firmware + signature)
    print(f"Writing signed firmware: {output_path}")
    with open(output_path, 'wb') as f:
        f.write(firmware)
        f.write(signature)

    total_size = firmware_size + sig_size
    print(f"Total size: {total_size:,} bytes ({total_size / 1024:.1f} KB)")

    # Verify signature if requested
    if verify:
        print("\nVerifying signature...")
        verify_signature(output_path, private_key_path)

    print("\n✅ Firmware signed successfully!")
    print(f"\nNext steps:")
    print(f"1. Upload to OTA server: {output_path}")
    print(f"2. Announce update with SHA256: {sha256_hash.hex()}")
    print(f"3. OTAManager will verify signature before applying update")

    return sha256_hash, signature


def verify_signature(signed_firmware_path, private_key_path):
    """Verify signature in signed firmware"""

    # Read signed firmware
    with open(signed_firmware_path, 'rb') as f:
        data = f.read()

    # Split firmware and signature (last 256 bytes)
    if len(data) < 256:
        print("❌ ERROR: File too small to contain signature")
        return False

    firmware = data[:-256]
    signature = data[-256:]

    firmware_size = len(firmware)
    print(f"Firmware size: {firmware_size:,} bytes")
    print(f"Signature size: {len(signature)} bytes")

    # Load public key from private key
    with open(private_key_path, 'rb') as f:
        private_key = serialization.load_pem_private_key(
            f.read(),
            password=None,
            backend=default_backend()
        )

    public_key = private_key.public_key()

    # Calculate hash
    sha256_hash = hashlib.sha256(firmware).digest()
    print(f"SHA256: {sha256_hash.hex()}")

    # Verify signature
    try:
        public_key.verify(
            signature,
            sha256_hash,
            padding.PKCS1v15(),
            hashes.SHA256()
        )
        print("✅ Signature verification PASSED")
        return True
    except Exception as e:
        print(f"❌ Signature verification FAILED: {e}")
        return False


def main():
    parser = argparse.ArgumentParser(
        description='Sign firmware binaries for secure OTA updates',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )

    parser.add_argument('firmware', help='Input firmware binary file')
    parser.add_argument('-o', '--output', help='Output signed firmware file')
    parser.add_argument('-k', '--key',
                       default='tools/keys/firmware_signing_key.pem',
                       help='Private key file (default: tools/keys/firmware_signing_key.pem)')
    parser.add_argument('-v', '--verify', action='store_true',
                       help='Verify signature after signing')

    args = parser.parse_args()

    # Resolve paths
    firmware_path = Path(args.firmware)
    if not firmware_path.exists():
        print(f"ERROR: Firmware file not found: {firmware_path}")
        sys.exit(1)

    private_key_path = Path(args.key)
    if not private_key_path.exists():
        print(f"ERROR: Private key not found: {private_key_path}")
        print(f"\nTo generate a new key pair:")
        print(f"  openssl genrsa -out {private_key_path} 2048")
        print(f"  openssl rsa -in {private_key_path} -pubout -out tools/keys/firmware_public_key.pem")
        sys.exit(1)

    output_path = Path(args.output) if args.output else firmware_path.with_suffix('.bin.signed')

    # Sign firmware
    try:
        sign_firmware(firmware_path, private_key_path, output_path, verify=args.verify)
    except Exception as e:
        print(f"\n❌ ERROR: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == '__main__':
    main()
