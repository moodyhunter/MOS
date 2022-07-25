#!/bin/bash

# get the current directory
DIR=$(pwd)

qemu-system-i386 -kernel $DIR/mos_multiboot.bin -monitor "unix:/tmp/monitor.sock,server,nowait" &
sleep 5

if pgrep -x "qemu-system-i386" >/dev/null; then
    echo "qemu-system-i386 is still running, probably the test has failed"
    printf '%s\n' "screendump failure.ppm" "q" | nc -U /tmp/monitor.sock
else
    echo "Test passed."
fi
