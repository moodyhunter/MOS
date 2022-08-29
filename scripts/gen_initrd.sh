#!/bin/bash

cd ./initrd
find . -depth | cpio -o --format=crc >../initrd.cpio
