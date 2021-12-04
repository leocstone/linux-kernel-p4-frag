#!/bin/bash

# The number of trials to perform
NUM_TRIALS=5

# The order of the number of pages to allocate 
# Example: if FRAG_ORDER_TO_ALLOC=10, 2^10 = 1024 pages
# will be allocated by each fragmentation code
# For reference:
# 22 --> (2^22)*4096 = 16 GiB
# 21 --> (2^21)*4096 = 8  GiB
# 20 --> (2^20)*4096 = 4  GiB
# 19 --> (2^19)*4096 = 2  GiB
# 18 --> (2^18)*4096 = 1  GiB
FRAG_ORDER_TO_ALLOC=22

SAMPLE_RATE_SECS=1
COMPACT_ORDER=4
COMPACT_THRESH_MIN=20
COMPACT_THRESH_MAX=80

# Let's sweep over the threshold parameter
for ((TRIAL_THRESH=$COMPACT_THRESH_MIN;TRIAL_THRESH<=$COMPACT_THRESH_MAX;TRIAL_THRESH+=10)); do

	for ((TRIAL=1;TRIAL<=$NUM_TRIALS;TRIAL+=1)); do
	
		sudo rmmod frag.ko
		sudo insmod frag.ko rate=$SAMPLE_RATE_SECS \
				    compaction_order=$COMPACT_ORDER \
				    compaction_thresh=$TRIAL_THRESH
		
		# Start recording and automatic compaction
		cat /proc/frag/record
		
		# Now lets launch the pgbench job to introduce more contention
		# Need to login to this postgres account and lauch it
		sudo runuser -l postgres -c 'pgbench -c 10 -j 6 -t 50000 example &'
		
		# Wait for the processes to start up
		sleep 5
		
		# Run the forced fragmentation codes, it will make
		# it so that the physical memory is forced to interleave the
		# allocatin of pages for each process, in-turn fragmenting the
		# the memory.
		./forced-frag/forceFrag $FRAG_ORDER_TO_ALLOC & \
		./forced-frag/forceFrag $FRAG_ORDER_TO_ALLOC
		
		# Wait for there 6 runs to finish...
		
		cat /proc/frag/last_recording >> ./rundata/order_${FRAG_ORDER_TO_ALLOC}_comporder_${COMPACT_ORDER}_compthresh_${TRIAL_THRESH}_rate_${SAMPLE_RATE_SECS}_trial_${TRIAL}_rundata.csv
		sudo rmmod frag.ko
		
		# Kill pgbench if it's still running
		sudo killall pgbench
	
	done
done

