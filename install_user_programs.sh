#!/bin/bash
# install_user_programs_clean.sh - Install user programs with cleanup

echo "╔══════════════════════════════════════════════════════════╗"
echo "║     Installing User Programs to disk.img                ║"
echo "╚══════════════════════════════════════════════════════════╝"
echo ""

# Create mount point
mkdir -p /tmp/oscomplex_mount

# Mount the FAT16 filesystem
echo "[1/5] Mounting disk.img..."
sudo mount -o loop disk.img /tmp/oscomplex_mount

if [ $? -ne 0 ]; then
    echo "ERROR: Failed to mount disk.img"
    exit 1
fi

# Create /bin directory if it doesn't exist
sudo mkdir -p /tmp/oscomplex_mount/bin

# Step 1: CLEAN OLD FILES
echo "[2/5] Cleaning old programs..."
echo "      Removing: HELLO USERTE~1 ASSIGN~1 ASSIGN~2 LS..."

# Remove old long-named files (they appear as ~1, ~2, etc)
sudo rm -f /tmp/oscomplex_mount/bin/HELLO 2>/dev/null
sudo rm -f /tmp/oscomplex_mount/bin/USERTE~1 2>/dev/null
sudo rm -f /tmp/oscomplex_mount/bin/ASSIGN~1 2>/dev/null
sudo rm -f /tmp/oscomplex_mount/bin/ASSIGN~2 2>/dev/null
sudo rm -f /tmp/oscomplex_mount/bin/LS 2>/dev/null

# Also remove any variations
sudo rm -f /tmp/oscomplex_mount/bin/USERTEST* 2>/dev/null
sudo rm -f /tmp/oscomplex_mount/bin/ASSIGNME* 2>/dev/null
sudo rm -f /tmp/oscomplex_mount/bin/ASSIGNM~* 2>/dev/null
sudo rm -f /tmp/oscomplex_mount/bin/assignment* 2>/dev/null
sudo rm -f /tmp/oscomplex_mount/bin/usertest* 2>/dev/null

# Step 2: Remove any leftover short names
sudo rm -f /tmp/oscomplex_mount/bin/hello 2>/dev/null
sudo rm -f /tmp/oscomplex_mount/bin/forktest 2>/dev/null
sudo rm -f /tmp/oscomplex_mount/bin/demo 2>/dev/null
sudo rm -f /tmp/oscomplex_mount/bin/demoreal 2>/dev/null
sudo rm -f /tmp/oscomplex_mount/bin/ls 2>/dev/null

echo "      ✓ Old files removed"

# Step 3: Copy NEW programs with SHORT names
echo "[3/5] Installing new programs..."

if [ ! -f user/hello.elf ]; then
    echo "      WARNING: user/hello.elf not found - did you build user programs?"
fi

sudo cp user/hello.elf /tmp/oscomplex_mount/bin/hello 2>/dev/null && echo "      ✓ hello"
sudo cp user/usertest_fork.elf /tmp/oscomplex_mount/bin/forktest 2>/dev/null && echo "      ✓ forktest"
sudo cp user/assignment_demo.elf /tmp/oscomplex_mount/bin/demo 2>/dev/null && echo "      ✓ demo"
sudo cp user/assignment_demo_real.elf /tmp/oscomplex_mount/bin/demoreal 2>/dev/null && echo "      ✓ demoreal"
sudo cp user/ls.elf /tmp/oscomplex_mount/bin/ls 2>/dev/null && echo "      ✓ ls"

# Step 4: Verify installation
echo "[4/5] Verifying installation..."
echo ""
echo "Programs in /bin/:"
sudo ls -1 /tmp/oscomplex_mount/bin/ | while read file; do
    echo "      • $file"
done

# Step 5: Unmount
echo ""
echo "[5/5] Unmounting disk.img..."
sudo umount /tmp/oscomplex_mount

if [ $? -eq 0 ]; then
    echo ""
    echo "╔══════════════════════════════════════════════════════════╗"
    echo "║     ✓ Installation Complete!                            ║"
    echo "╚══════════════════════════════════════════════════════════╝"
    echo ""
    echo "Available programs:"
    echo "  exec /bin/hello      - Simple hello world"
    echo "  exec /bin/forktest   - Fork/wait basic test"
    echo "  exec /bin/demo       - Assignment demo (simulated ls)"
    echo "  exec /bin/demoreal   - Assignment demo (real exec)"
    echo "  exec /bin/ls         - List files"
    echo ""
    echo "Old files (ASSIGN~1, USERTE~1, etc.) have been removed."
    echo ""
else
    echo "ERROR: Failed to unmount disk.img"
    exit 1
fi