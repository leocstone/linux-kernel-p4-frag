#!/bin/bash

# The number of trials to perform
NUM_TRIALS=10

# The order of the number of pages to allocate 
# Example: if ORDER_TO_ALLOC=10, 2^10 = 1024 pages
# will be allocated by each fragmentation code
# For reference:
# 21 --> (2^21)*4096 = 8GiB
# 20 --> (2^20)*4096 = 4GiB
# 19 --> (2^19)*4096 = 2GiB
# 18 --> (2^18)*4096 = 1GiB
ORDER_TO_ALLOC=19

for ((TRIAL=1;TRIAL<=$NUM_TRIALS;TRIAL+=1)); do

	sudo rmmod frag.ko
	sudo insmod frag.ko rate=3 compaction_order=4 compaction_thresh=20
	
	# Start recording and automatic compaction
	cat /proc/frag/record
	
	# Now lets launch the pgbench job to introduce more contention
	# Need to login to this postgres account and lauch it
	sudo runuser -l postgres -c 'pgbench -c 10 -j 6 -t 50000 example &'
	
	# Wait for the processes to start up
	sleep 5
	
	# Run eight of the forced fragmentation codes, it will make
	# it so that the physical memory is forced to interleave the
	# allocatin of pages for each process, in-turn fragmenting the
	# the memory.
	./forced-frag/forceFrag $ORDER_TO_ALLOC & \
	./forced-frag/forceFrag $ORDER_TO_ALLOC & \
	./forced-frag/forceFrag $ORDER_TO_ALLOC & \
	./forced-frag/forceFrag $ORDER_TO_ALLOC & \
	./forced-frag/forceFrag $ORDER_TO_ALLOC & \
	./forced-frag/forceFrag $ORDER_TO_ALLOC & \
	./forced-frag/forceFrag $ORDER_TO_ALLOC & \
	./forced-frag/forceFrag $ORDER_TO_ALLOC 
	
	# Wait for there 6 runs to finish...
	
	cat /proc/frag/last_recording >> order_${ORDER_TO_ALLOC}_trial_${TRIAL}_rundata.csv
	sudo rmmod frag.ko
	
	# Kill pgbench if it's still running
	sudo killall pgbench

done

