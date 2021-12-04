#!/bin/bash

sudo rmmod frag.ko
sudo insmod frag.ko

# Start recording and automatic compaction
cat /proc/frag/record

# Now lets launch the pgbench job to introduce more contention
# Need to login to this postgres account and lauch it
sudo su - postgres
nohup pgbench -c 10 -j 6 -t 50000 example & 
exit

echo "out of pgbench launch"

# The order of the number of pages to allocate 
# Example: if ORDER_TO_ALLOC=10, 2^10 = 1024 pages
# will be allocated by each fragmentation code
ORDER_TO_ALLOC=20

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

# Wait for there 6 runs to finish...

# Now let's run the module and see what happens 
# before and after manual compaction
#sudo insmod frag.ko
#cat /proc/frag/info
#sudo rmmod frag.ko

#dmesg | tail -n 15

cat /proc/frag/last_recording >> order_${ORDER_TO_ALLOC}_rundata.csv
sudo rmmod frag.ko

#cat /proc/frag/record

