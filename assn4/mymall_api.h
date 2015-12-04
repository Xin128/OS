#ifndef MALLOC_API_H
#define MALLOC_API_H

#define FIRST_FIT 0
#define BEST_FIT  1
#define USED 1
#define FREE 0
#define LAST_BLOCK_MAX_BYTES 1280   // max num bytes - set as such, rather than 128 kb, for testing purposes
#define MIN_EXCESS_SPLIT 10
#define TAG_SIZE 8

extern char my_malloc_error[100];

void* my_malloc(int size);
int my_free(void *ptr);
void my_mallopt(int policy);
void my_mallinfo();

#endif
