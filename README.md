# LKP Project 4 Fragmentation Measurement Module

# Description
This module periodically samples information from the buddy allocator in order to measure memory fragmentation, records these samples in an internal list, and outputs them in a CSV format.

# Building
Once you have your kernel source installed, just enter the project directory and run `make`.

# Directions
This module creates a new directory in `/proc`; `/proc/frag`. 
The entries in this directory are the primary user interface to the module. 

`/proc/frag/info` will produce a list of free blocks in buddy allocators for each zone when read.

`/proc/frag/record` will begin or stop recording fragmentation data when opened (so you can just `cat /proc/frag/record` to start/stop recording).
Note that starting a new recording will delete the old one.

`/proc/frag/last_recording` will produce the contents of the internally recorded buffer in CSV format when read. 
Each sample contains a Unix timestamp, followed by, for every order in the buddy allocator, the unusable free space metric and the number of free blocks for that order.
