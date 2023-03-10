#!/bin/bash

# get the current directory
DIR=$(pwd)

qemu-system-i386 \
    -kernel $DIR/mos_multiboot.bin \
    -initrd $DIR/initrd.cpio \
    -m 4G \
    -smp 1 \
    -monitor "unix:/tmp/monitor.sock,server,nowait" \
    -nographic \
    -chardev stdio,id=char0,logfile=test-failure.log,signal=off \
    -append "poweroff_on_panic=true $*" \
    -serial chardev:char0

PANIC_MARKER="!!!!! KERNEL PANIC !!!!!"

if grep -q "$PANIC_MARKER" test-failure.log; then
    echo "Kernel panic detected."
    echo "Test failed."
    echo "See:"
    echo " - test-failure.bin   the kernel binary"
    echo " - test-failure.log   the serial log"
    exit 1
fi

echo "Test passed."
