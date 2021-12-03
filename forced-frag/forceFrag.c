

#include <stdio.h>
#include <unistd.h>
#include <string.h>

#define PAGE_SIZE_IN_BYTES 4096

int main(int argc, char** argv){


	char *p;
	size_t i;
	int order;
	unsigned long numpages;

	if(argc != 2){
		fprintf(stderr, "Not enough arguments! Provided %d, need 1\n", argc-1);
		return -1;
	}

	if (sscanf (argv[1], "%i", &order) != 1) {
    		fprintf(stderr, "error - not an integer\n");
		return -1;
	}

	numpages = 1 << order;
	
	printf("Using order %d, creating %lu pages\n", order, numpages);

	// Get the end position of this process's data segment
	p = sbrk(0);

	for(i = 0; i < numpages; i++){
		// Extend the process data segment by 1 page
		// Though this appears contiguous in virtual memory
		// it may not be at the physical memory level
		sbrk(PAGE_SIZE_IN_BYTES);

		// After the extension, let's write a character
		// to fill up the new memory
		memset(p + (i*PAGE_SIZE_IN_BYTES), 'a', PAGE_SIZE_IN_BYTES);
	}

	printf("Forced Fragmentation Complete!\n");
	return 0;
}
