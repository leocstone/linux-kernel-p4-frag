#!/bin/bash

sudo insmod frag.ko
cat /proc/frag/info
sudo rmmod frag.ko
#dmesg | tail -n 15

