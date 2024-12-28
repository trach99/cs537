#!/bin/bash

rm disk1 disk2
make
dd if=/dev/zero of=disk1 bs=1M count=1
dd if=/dev/zero of=disk2 bs=1M count=1
./mkfs -r 1 -d disk1 -d disk2 -i 32 -b 200
./wfs disk1 disk2 -s -f mnt