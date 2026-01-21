#!/bin/bash
# install_user_programs.sh - Copy user programs to disk.img

echo "Installing user programs to disk.img..."

# Create mount point
mkdir -p /tmp/oscomplex_mount

# Mount the FAT16 filesystem
sudo mount -o loop disk.img /tmp/oscomplex_mount

# Create /bin directory if it doesn't exist
sudo mkdir -p /tmp/oscomplex_mount/bin

# Copy user programs
echo "Copying hello.elf to /bin/hello..."
sudo cp user/hello.elf /tmp/oscomplex_mount/bin/hello

echo "Copying usertest_fork.elf to /bin/usertest_fork..."
sudo cp user/usertest_fork.elf /tmp/oscomplex_mount/bin/usertest_fork

# List what's on the disk
echo ""
echo "Files on disk:"
sudo ls -lh /tmp/oscomplex_mount/
sudo ls -lh /tmp/oscomplex_mount/bin/ 2>/dev/null || echo "No /bin directory yet"

# Unmount
sudo umount /tmp/oscomplex_mount

echo ""
echo "✓ User programs installed!"
echo "You can now run:"
echo "  exec /bin/hello"
echo "  exec /bin/usertest_fork"