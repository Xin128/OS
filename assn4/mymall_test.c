#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "mymall_api.h"

/* TEST -------------------------------------------------------------------------------------------- */

int main()
{
    char* data;
    int i, b=0, f=0, errors=0;
    char* ptr[10];

    printf("\nBegin test, program break at %p, all byte counts should = 0:\n", sbrk(0));
    my_mallinfo();
    b = 10;
    printf("\nMallocing %d bytes:\n", b);
    data = (char*) my_malloc(10);
    if(data == NULL) {
	printf("%s", my_malloc_error);
  	errors++;
    }
    if (errors > 0)
	printf("Total error count = %d\n", errors);

    printf("Should have %d bytes allocated, %d bytes free space, %d bytes contiguous:\n", b, f, f);
    my_mallinfo();
    printf("Heap now ends at %p\n", sbrk(0));

    char *tmp = "datatest 1";
    memcpy(data, tmp, 10);

    printf("Output from %p should read \"%s\":  ", data, tmp);
    for(i = 0; i < 10; i++) {
	printf("%c", data[i]);
        if(data[i] != *(tmp+i))
            errors++;
    }

    if (errors > 0)
	printf("\nTotal error count = %d\n", errors);

    printf("\n\n\nFreeing just-allocated data at address %p...\n", data);
    if (my_free(data) < 0) {
	printf("%s", my_malloc_error);
	errors++;
    }
    else
	b -= 10;

    if (errors > 0)
	printf("\nTotal error count = %d\n", errors);

    f = 10;
    printf("Should have %d bytes allocated, %d bytes free space:\n", b, f);
    my_mallinfo();

    printf("\nAllocating more blocks...\n");
    for(i = 0; i < 10; i++) {
        ptr[i] = my_malloc((10-i)*10);
        if(ptr == NULL) {
	    printf("%s", my_malloc_error);
	    errors++;
	}
	else {
	    if ((10-i)*10 <= f)
		f = 0;
	    b += (10-i)*10;
	}
    }

    printf("\nNow should have %d bytes allocated, %d bytes free.\n", b, f);
    my_mallinfo();
    printf("Program break should be at %p:  %p\n\n", ptr[8]+20+8, sbrk(0));

    printf("\nFreeing 100-byte block... ");
    if (my_free(ptr[0]) < 0) {
	printf("%s", my_malloc_error);
	errors++;
    }
    else {
	b -= 100;
	f += 100;
    }
    printf("Now freeing 90-byte block...\n");
    if (my_free(ptr[1]) < 0) {
	printf("%s", my_malloc_error);
	errors++;
    }
    else {
	b -= 90;
	f += 90;
	f += 16;	// due to merging
    }

    if (errors > 0)
	printf("\nTotal error count = %d\n", errors);

    printf("\nNow should have %d bytes allocated, %d bytes free, %d bytes contiguous.\n", b, f, f);
    my_mallinfo();

    printf("\nFreeing 70-byte block... ");
    if (my_free(ptr[3]) < 0) {
	printf("%s", my_malloc_error);
	errors++;
    }
    else {
	b -= 70;
	f += 70;
    }
    printf("Now freeing 10-byte block...\n");
    if (my_free(ptr[9]) < 0) {
	printf("%s", my_malloc_error);
	errors++;
    }
    else {
    	b -= 10;
    	f += 10;
    	f += 16;	// due to merging
    }
    if (errors > 0)
	printf("\nTotal error count = %d\n", errors);
    printf("\nNow should have %d bytes allocated, %d bytes free, %d bytes contiguous.\n", b, f, (f-70));
    my_mallinfo();

    printf("\nAllocating a memory block of 10 bytes.. by the first fit method,\nshould grab the first memory block of 206 bytes; since that's\nsplittable, the block splits into an allocatable portion and a free portion.\n\n");
    ptr[0] = (char*) my_malloc(10);
    if(ptr[0] == NULL) {
	printf("%s", my_malloc_error);
	errors++;
    }
    else {
	b += 10;
	f -= (232 - 206);
    }
    if (errors > 0)
	printf("Total error count = %d\n", errors);

    printf("Should have %d bytes allocated, %d bytes free space, %d bytes contiguous.\n", b, f, (f-70));
    my_mallinfo();

    printf("\nNow freeing 30-byte block...\n");
    if (my_free(ptr[7]) < 0) {
	printf("%s", my_malloc_error);
	errors++;
    }
    else {
    	b -= 30;
    	f += 30;
    }
    if (errors > 0)
	printf("\nTotal error count = %d\n", errors);
    printf("\nNow should have %d bytes allocated, %d bytes free, %d bytes contiguous.\n", b, f, (f-70-30));
    my_mallinfo();

    printf("\nSwitching method to best-fit...\n");
    my_mallopt(BEST_FIT);
    printf("Allocating a new 10 byte block.. by best-fit, should be at address %p\n", ptr[7]);

    ptr[1] = (char*) my_malloc(10);
    if(ptr[1] == NULL) {
	printf("%s", my_malloc_error);
  	errors++;
    }
    else {
	b += 10;
	f -= 30;
    }
    if (errors > 0)
	printf("Total error count = %d\n", errors);

    printf("\nFree block is 30 bytes, but not split, too small; so..");
    printf("\nShould have %d bytes allocated, %d bytes free, %d bytes contiguous.\n", b, f, (f-70));
    my_mallinfo();

    void* end = sbrk(0);
    printf("\nAllocating new 2000-byte block.\n");
    ptr[3] = (char*) my_malloc(2000);
    if(ptr[3] == NULL) {
	printf("%s", my_malloc_error);
  	errors++;
    }
    else
	b += 2000;
    if (errors > 0)
	printf("Total error count = %d\n", errors);

    printf("\nShould have %d bytes allocated, %d bytes free, %d bytes contiguous.\n", b, f, (f-70));
    my_mallinfo();

    printf("\nNo current free blocks large enough, so should allocate new bytes\nat end of heap:");
    printf("\nProgram break before allocation: %p\nand after allocation, should be %p: %p\n", end, (end+2000+2*TAG_SIZE), sbrk(0));

    printf("\n\nNow freeing 2000-byte block...\n");
    if(my_free(ptr[3]) < 0) {
	printf("%s", my_malloc_error);
  	errors++;
    }
    else
	b -= 2000;
    if (errors > 0)
	printf("Total error count = %d\n", errors);

    printf("\nFreed memory at end of stack now larger than max allowable\nfreed memory at end of stack, so heap should decrease:");
    printf("\nShould have %d bytes allocated, %d bytes free, %d bytes contiguous.\n", b, f, (f-70));
    my_mallinfo();

    printf("After freeing, current rogram break should be %p: %p\n", end, sbrk(0));

    printf("\n\nAttempting to free just-freed pointer, which now points\nabove the program break - should FAIL:\n");
    if(my_free(ptr[3]) < 0)
	printf("%s", my_malloc_error);
    else
	errors++;
    if (errors > 0)
	printf("Total error count = %d\n", errors);

    printf("\n\n--- DONE (%d errors) --- \n\n\n", errors);

    return errors;
}
