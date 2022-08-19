#!/bin/bash

# get the current directory
DIR=$(pwd)

qemu-system-i386 -kernel $DIR/mos_multiboot.bin \
    -m 4G \
    -monitor "unix:/tmp/monitor.sock,server,nowait" \
    -nographic \
    -chardev stdio,id=char0,logfile=test-failure.log,signal=off \
    -append "mos_tests" \
    -serial chardev:char0 &

pid=$!
sleep 30

ps -p $pid >/dev/null

[ $? == 1 ] && echo "Test passed." && exit 0

echo "qemu-system-i386 is still running, probably the test has failed."
printf '%s\n' "screendump test-failure.ppm" "dump-guest-memory test-failure.dmp" "q" | nc -U /tmp/monitor.sock
echo "Test failed."
echo "See:"
echo " - test-failure.bin   the kernel binary"
echo " - test-failure.ppm   the screen dump"
echo " - test-failure.dmp   the guest memory dump"
echo " - test-failure.log   the serial log"
exit 1
