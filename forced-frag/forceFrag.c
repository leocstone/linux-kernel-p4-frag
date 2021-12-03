

#include <stdio.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char** argv){

	printf("Forcing Fragmentation!\n");

	char *p;
	size_t i;

	// Get the end position of this process's data segment
	p = sbrk(0);

	for(i = 0; i < 1000000; i++){
		// Extend the process data segment by 1 page
		// Though this appears contiguous in virtual memory
		// it may not be at the physical memory level
		sbrk(4096);

		// After the extension, let's write a character
		// to fill up the new memory
		memset(p + (i*4096), 'a', 4096);
	}

	printf("Forced Fragmentation Complete!\n");
	return 0;
}
