#!/bin/bash

# The order of the number of pages to allocate 
# Example: if ORDER_TO_ALLOC=10, 2^10 = 1024 pages
# will be allocated by each fragmentation code
ORDER_TO_ALLOC=21

# Run all six at the same time to force a little fragmentation
# These six correspond to the number of cores on our test cpu
# The idea is that all of these programs will force fragmentation
# by extending their data segments at the same times, forcing
# the physical memory to interleave the allocations.
./forced-frag/forceFrag $ORDER_TO_ALLOC & \
./forced-frag/forceFrag $ORDER_TO_ALLOC & \
./forced-frag/forceFrag $ORDER_TO_ALLOC & \
./forced-frag/forceFrag $ORDER_TO_ALLOC & \
./forced-frag/forceFrag $ORDER_TO_ALLOC & \
./forced-frag/forceFrag $ORDER_TO_ALLOC

# Now let's run the module and see what happens 
# before and after manual compaction
sudo insmod frag.ko
cat /proc/frag/info
sudo rmmod frag.ko
#dmesg | tail -n 15

