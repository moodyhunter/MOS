#!/usr/bin/env bash

set -e

# check if MOS_BUILD_DIR is set
if [ -z "$MOS_BUILD_DIR" ]; then
    echo "MOS_BUILD_DIR is not set"
    exit 1
fi

if [ ! -d "$MOS_SRC_DIR" ]; then
    echo "MOS_SRC_DIR is not set"
    exit 1
fi

# check if limine-deploy is installed
if ! command -v limine-deploy &>/dev/null; then
    echo "limine-deploy could not be found"
    exit 1
fi

# check if xorriso is installed
if ! command -v xorriso &>/dev/null; then
    echo "xorriso could not be found"
    exit 1
fi

LIMINE_ROOT="/usr/share/limine"

BOOT_DIR="$MOS_BUILD_DIR/boot.dir"
ISO_ROOT="$BOOT_DIR/iso_root/limine"

LIMINE_CONFIG="$MOS_SRC_DIR/arch/x86/boot/limine/limine.cfg"
if [ ! -f "$LIMINE_CONFIG" ]; then
    echo "limine.cfg could not be found"
    exit 1
fi

mkdir -p "$ISO_ROOT"
mkdir -p "$ISO_ROOT/EFI/BOOT"

cp "$LIMINE_ROOT/limine.sys" "$LIMINE_ROOT/limine-cd.bin" "$LIMINE_ROOT/limine-cd-efi.bin" "$ISO_ROOT/"
cp $LIMINE_ROOT/BOOT*.EFI "$ISO_ROOT/EFI/BOOT/"

# copy kernel, initrd and limine_entry.elf
cp "$BOOT_DIR/limine_entry.elf" "$ISO_ROOT/"
cp "$MOS_BUILD_DIR/initrd.cpio" "$ISO_ROOT/"
cp "$LIMINE_CONFIG" "$ISO_ROOT/"

xorriso \
    -as mkisofs \
    -b limine-cd.bin \
    -no-emul-boot \
    -boot-load-size 4 \
    -boot-info-table \
    --efi-boot limine-cd-efi.bin \
    -efi-boot-part \
    --efi-boot-image \
    --protective-msdos-label \
    "$ISO_ROOT" \
    -o "$BOOT_DIR/mos_limine.iso"

limine-deploy "$BOOT_DIR/mos_limine.iso"
