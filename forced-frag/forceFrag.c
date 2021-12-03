

#include <stdio.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char** argv){


	char *p;
	size_t i;
	int numpages = 10000000;

	if(argc != 2){
		fprintf(stderr, "Not enough arguments! Provided %d, need 1\n", argc-1);
		return -1;
	}

	printf("Forcing Fragmentation!\n");

	if (sscanf (argv[1], "%i", &numpages) != 1) {
    		fprintf(stderr, "error - not an integer\n");
		return -1;
	}

	printf("Creating %d pages\n", numpages);

	// Get the end position of this process's data segment
	p = sbrk(0);

	for(i = 0; i < numpages; i++){
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
