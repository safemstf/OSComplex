#!/bin/bash
# Quick start script for LinuxComplex development

echo "======================================"
echo "  LinuxComplex - Quick Start"
echo "======================================"
echo ""

# Check if tools are installed
echo "[*] Checking prerequisites..."
MISSING=""

if ! command -v x86_64-linux-gnu-gcc &> /dev/null; then
    MISSING="$MISSING x86_64-linux-gnu-gcc"
fi

if ! command -v qemu-system-x86_64 &> /dev/null; then
    MISSING="$MISSING qemu-system-x86_64"
fi

if [ -n "$MISSING" ]; then
    echo "[-] Missing tools:$MISSING"
    echo ""
    echo "Install with:"
    echo "  sudo apt install gcc-x86-64-linux-gnu binutils-x86-64-linux-gnu qemu-system-x86"
    exit 1
fi

echo "[+] All prerequisites installed!"
echo ""

# Build
echo "[*] Building LinuxComplex kernel..."
make clean
make linuxcomplex.bin

if [ $? -eq 0 ]; then
    echo ""
    echo "[+] Build successful!"
    echo ""
    echo "Run with: make run"
    echo "  or:     qemu-system-x86_64 -kernel linuxcomplex.bin"
    echo ""
    echo "For a graphical window, try:"
    echo "  qemu-system-x86_64 -kernel linuxcomplex.bin -vga std"
    echo ""
else
    echo "[-] Build failed. Check errors above."
    exit 1
fi
