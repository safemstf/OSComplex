#!/bin/bash
# install_user_programs.sh - Install user programs to disk.img

echo ""
echo "========================================="
echo "  Installing User Programs to disk.img"
echo "========================================="
echo ""

# Create mount point
mkdir -p /tmp/oscomplex_mount

# Mount the FAT16 filesystem
echo "[1/4] Mounting disk.img..."
sudo mount -o loop disk.img /tmp/oscomplex_mount

if [ $? -ne 0 ]; then
    echo "ERROR: Failed to mount disk.img"
    echo "Make sure disk.img exists and you have sudo access."
    exit 1
fi

# Create /bin directory if it doesn't exist
sudo mkdir -p /tmp/oscomplex_mount/bin

# Clean old programs
echo "[2/4] Cleaning old programs..."
sudo rm -f /tmp/oscomplex_mount/bin/* 2>/dev/null
echo "      Done"

# Copy new programs
echo "[3/4] Installing programs..."

PROGRAMS="hello counter sysinfo spin test"
INSTALLED=0

for prog in $PROGRAMS; do
    if [ -f "user/${prog}.elf" ]; then
        sudo cp "user/${prog}.elf" "/tmp/oscomplex_mount/bin/${prog}"
        if [ $? -eq 0 ]; then
            echo "      + ${prog}"
            INSTALLED=$((INSTALLED + 1))
        else
            echo "      ! Failed: ${prog}"
        fi
    else
        echo "      ? Not found: user/${prog}.elf"
    fi
done

# Verify and list
echo ""
echo "[4/4] Installed programs:"
sudo ls -la /tmp/oscomplex_mount/bin/ | tail -n +2 | while read line; do
    echo "      $line"
done

# Unmount
echo ""
sudo umount /tmp/oscomplex_mount

if [ $? -eq 0 ]; then
    echo "========================================="
    echo "  Installation Complete! ($INSTALLED programs)"
    echo "========================================="
    echo ""
    echo "Usage in OSComplex shell:"
    echo "  exec /bin/hello    - Simple hello world"
    echo "  exec /bin/counter  - Count 1-10 with delays"
    echo "  exec /bin/sysinfo  - Display system info"
    echo "  exec /bin/spin     - Spinner animation"
    echo "  exec /bin/test     - Basic syscall tests"
    echo ""
else
    echo "ERROR: Failed to unmount disk.img"
    exit 1
fi
