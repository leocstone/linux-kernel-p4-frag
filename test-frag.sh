#!/bin/bash

# Run all three at the same time to force a little fragmentation
./forced-frag/forceFrag & ./forced-frag/forceFrag & ./forced-frag/forceFrag

# Now let's run the module and see what happens 
# before and after manual compaction
sudo insmod frag.ko
cat /proc/frag/info
sudo rmmod frag.ko
#dmesg | tail -n 15

