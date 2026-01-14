#!/bin/bash
# verify_multiboot.sh - Verify multiboot header is in the correct location

echo "=== Multiboot Header Verification ==="
echo ""

# Check ELF file
if [ -f "linuxcomplex.bin.elf" ]; then
    echo "✓ ELF file exists"
    
    # Check entry point
    entry=$(readelf -h linuxcomplex.bin.elf | grep "Entry point" | awk '{print $4}')
    echo "  Entry point: $entry"
    
    # Check for .multiboot or .boot section
    echo "  Sections:"
    readelf -S linuxcomplex.bin.elf | grep -E "\.boot|\.multiboot" | head -5
    
    echo ""
    echo "  First 32 bytes of binary (should show multiboot magic):"
    hexdump -C linuxcomplex.bin.elf | head -3
else
    echo "✗ ELF file not found"
fi

echo ""

# Check flat binary
if [ -f "linuxcomplex.bin" ]; then
    echo "✓ Flat binary exists"
    echo "  First 32 bytes (should contain 0x1BADB002):"
    hexdump -C linuxcomplex.bin | head -3
    
    # Search for multiboot magic
    if xxd -p linuxcomplex.bin | tr -d '\n' | grep -q "02b0ad1b"; then
        echo "  ✓ Multiboot magic found (0x1BADB002 in little-endian)"
        pos=$(xxd linuxcomplex.bin | grep -b "1bad" | head -1 | cut -d: -f1)
        echo "    Position: $pos bytes from start"
    else
        echo "  ✗ Multiboot magic NOT found!"
    fi
else
    echo "✗ Flat binary not found"
fi

echo ""
echo "=== Recommendation ==="
echo "Try: qemu-system-i386 -kernel linuxcomplex.bin.elf -m 32M"