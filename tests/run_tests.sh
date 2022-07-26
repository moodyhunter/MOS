#!/bin/bash

# get the current directory
DIR=$(pwd)

qemu-system-i386 -kernel $DIR/mos_multiboot.bin -monitor "unix:/tmp/monitor.sock,server,nowait" -nographic &
pid=$!
sleep 5

ps -p $pid >/dev/null

[ $? == 1 ] && echo "Test passed." && exit 0

echo "qemu-system-i386 is still running, probably the test has failed."
printf '%s\n' "screendump test-failure.ppm" "dump-guest-memory test-failure.dmp" "q" | nc -U /tmp/monitor.sock
echo "Test failed."
echo "See test-failure.ppm for the screen dump, and test-failure.dmp for the guest memory dump."
exit 1
