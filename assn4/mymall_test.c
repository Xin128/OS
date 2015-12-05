#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "mymall_api.h"

int main()
{
    char* data;
    int i, b=0, f=0, c=0, errors=0;
    char* ptr[20];

    void *original = sbrk(0);
    printf("\n\n\nBegin test, program break at %p, all byte counts should = 0:\n", original);
    my_mallinfo();


    b = 10;
    printf("\n\nMallocing %d bytes:\n", b);
    data = (char*) my_malloc(10);
    if(data == NULL) {
	printf("%s", my_malloc_error);
  	errors++;
    }
    if (errors > 0)
	printf("Total error count = %d\n", errors);

    printf("Should have %d bytes allocated, %d bytes free space, %d bytes contiguous:\n", b, f, c);
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

    printf("\n\n\n\nFreeing just-allocated data at address %p...\n", data);
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
    printf("\n\nAllocating more blocks...\n");
    for(i = 0; i < 20; i++) {
        ptr[i] = my_malloc((20-i)*10);
        if(ptr == NULL) {
	    printf("%s", my_malloc_error);
	    errors++;
	}
	else {
	    if ((20-i)*10 <= f)
		f = 0;
	    b += (20-i)*10;
	}
    }

    printf("\nNow should have %d bytes allocated, %d bytes free.\n", b, f);
    my_mallinfo();
    printf("Program break should be at %p:  %p\n\n", ptr[18]+20+8, sbrk(0));


    printf("\n\nFreeing all blocks...\n");
    if (my_free(ptr[19]) < 0) {
        printf("%s", my_malloc_error);
        errors++;
    }
    else
        b -= 10;
    for (i = 0; i < 18; i++) {
        if (my_free(ptr[i]) < 0) {
            printf("%s", my_malloc_error);
            errors++;
        }
    	  else
    	      b -= (20-i)*10;
    }


    if (my_free(ptr[18]) < 0) {
        printf("%s", my_malloc_error);
        errors++;
    }
    else
        b -= 20;

    if (errors > 0)
        printf("\nTotal error count = %d\n", errors);
        printf("\nShould have %d bytes allocated, %d bytes free.\n", b, f);
        my_mallinfo();
        printf("Program break should be at %p:  %p\n\n", original, sbrk(0));


    printf("\n\nReallocating old blocks...\n");
    for(i = 0; i < 20; i++) {
        ptr[i] = my_malloc((20-i)*10);
        if(ptr == NULL) {
    	      printf("%s", my_malloc_error);
    	      errors++;
    	}
    	else {
    	    if ((20-i)*10 <= f)
		f += 0;
    	    b += (20-i)*10;
    	}
    }

    printf("\nNow should again have %d bytes allocated, %d bytes free.\n", b, f);
    my_mallinfo();
    printf("Program break should be at %p:  %p\n\n", ptr[19]+10+8, sbrk(0));



    printf("\n\nFreeing 200-byte block... ");
    if (my_free(ptr[0]) < 0) {
	printf("%s", my_malloc_error);
	errors++;
    }
    else {
	b -= 200;
	f += 200;
    }
    printf("Now freeing 190-byte block...\n");
    if (my_free(ptr[1]) < 0) {
	printf("%s", my_malloc_error);
	errors++;
    }
    else {
	b -= 190;
	f += 190;
	f += 16;	// due to merging
  	c = f;
    }

    if (errors > 0)
	printf("\nTotal error count = %d\n", errors);

    printf("\nNow should have %d bytes allocated, %d bytes free, %d bytes contiguous.\n", b, f, c);
    my_mallinfo();

    printf("\n\nFreeing 170-byte block... ");
    if (my_free(ptr[3]) < 0) {
	printf("%s", my_malloc_error);
	errors++;
    }
    else {
	b -= 170;
	f += 170;
    }
    printf("Now freeing 110-byte block...\n");
    if (my_free(ptr[9]) < 0) {
	printf("%s", my_malloc_error);
	errors++;
    }
    else {
    	b -= 110;
    	f += 110;
    }
    if (errors > 0)
	printf("\nTotal error count = %d\n", errors);
    printf("\nNow should have %d bytes allocated, %d bytes free, %d bytes contiguous.\n", b, f, c);
    my_mallinfo();

    printf("\n\nAllocating a memory block of 10 bytes.. by the first fit method,\nshould grab the first memory block of 406 bytes; since that's\nsplittable, the block splits into an allocatable portion and a free portion.\n\n");
    ptr[0] = (char*) my_malloc(10);
    if(ptr[0] == NULL) {
	printf("%s", my_malloc_error);
	errors++;
    }
    else {
	b += 10;
	f -= 26;
	c -= 26;
    }
    if (errors > 0)
	printf("Total error count = %d\n", errors);

    printf("Should have %d bytes allocated, %d bytes free space, %d bytes contiguous.\n", b, f, c);
    my_mallinfo();

    printf("\n\nNow freeing 130-byte block...\n");
    if (my_free(ptr[7]) < 0) {
	printf("%s", my_malloc_error);
	errors++;
    }
    else {
    	b -= 130;
    	f += 130;
    }
    if (errors > 0)
	printf("\nTotal error count = %d\n", errors);
    printf("\nNow should have %d bytes allocated, %d bytes free, %d bytes contiguous.\n", b, f, c);
    my_mallinfo();

    printf("\n\nSwitching method to best-fit...\n");
    my_mallopt(BEST_FIT);
    printf("Allocating a new 10 byte block.. by best-fit, should be at address %p\n", ptr[9]);


    ptr[1] = (char*) my_malloc(10);
    if(ptr[1] == NULL) {
	printf("%s", my_malloc_error);
  	errors++;
    }
    else {
	b += 10;
	f -= 26;
    }
    if (errors > 0)
	printf("Total error count = %d\n", errors);

    printf("\nFree block is 30 bytes, but not split, too small; so..");
    printf("\nShould have %d bytes allocated, %d bytes free, %d bytes contiguous.\n", b, f, c);
    my_mallinfo();

    void* end = sbrk(0);
    printf("\n\nAllocating new 2000-byte block.\n");
    ptr[3] = (char*) my_malloc(2000);
    if(ptr[3] == NULL) {
	printf("%s", my_malloc_error);
  	errors++;
    }
    else
	b += 2000;
    if (errors > 0)
	printf("Total error count = %d\n", errors);

    printf("\nShould have %d bytes allocated, %d bytes free, %d bytes contiguous:\n", b, f, c);
    my_mallinfo();

    printf("\n\nNo current free blocks large enough, so should allocate new bytes at end of heap:");
    printf("\nProgram break before allocation: %p\nand after allocation, should be %p: %p\n", end, (end+2000+2*TAG_SIZE), sbrk(0));

    printf("\n\n\nNow freeing 2000-byte block...\n");
    if(my_free(ptr[3]) < 0) {
	printf("%s", my_malloc_error);
  	errors++;
    }
    else
	b -= 2000;
    if (errors > 0)
	printf("Total error count = %d\n", errors);

    printf("\nFreed memory at end of stack now larger than max allowable\nfreed memory at end of stack, so heap should decrease:");
    printf("\nShould have %d bytes allocated, %d bytes free, %d bytes contiguous:\n", b, f, c);
    my_mallinfo();

    printf("After freeing, current program break should be %p: %p\n", end, sbrk(0));

    printf("\n\n\nAttempting to free just-freed pointer, which now points\nabove the program break - should FAIL:\n");
    if(my_free(ptr[3]) < 0)
	printf("%s", my_malloc_error);
    else
	errors++;
    if (errors > 0)
	printf("Total error count = %d\n", errors);

    printf("\n\n--- DONE (%d errors) --- \n\n\n", errors);

    return errors;
}
