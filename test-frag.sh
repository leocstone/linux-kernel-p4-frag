#!/bin/bash

# The number of trials to perform
NUM_TRIALS=10

# The order of the number of pages to allocate 
# Example: if ORDER_TO_ALLOC=10, 2^10 = 1024 pages
# will be allocated by each fragmentation code
ORDER_TO_ALLOC=21

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
	
	cat /proc/frag/last_recording >> order_${ORDER_TO_ALLOC}_trial_${TRIAL}_rundata.csv
	sudo rmmod frag.ko
	
	# Kill pgbench if it's still running
	sudo killall pgbench

done

