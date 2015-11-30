/* Author: Lara Goxhaj  - 260574574
 * COMP 310/ECSE 427
 * Malloc implementation API (with test included in main method)
 *     Supports first-fit and best-fit allocation
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "mapi.h"

/* global error string var, set by my_malloc if memory could not be allocated */
char my_malloc_error[100];

typedef struct tag_t {
    int length;
    int stat;
} tag_t;

typedef struct frblk_t frblk_t;
struct frblk_t {
    tag_t tag;
    frblk_t *prev;
    frblk_t *next;
};

frblk_t* frblk_list;     	// doubly linked list holds all free blocks
int alloc_policy = FIRST_FIT;   // default
int total_alloc_bytes = 0;
int total_free_bytes = 0;

void set_end_tag(tag_t* itag);
tag_t* get_end_tag(tag_t* itag);
void merge_blocks(frblk_t* first, frblk_t* second);


/* TEST -------------------------------------------------------------------------------------------- */

int main() 
{
    char* data;
    int i, b=0, f=0, c=0, errors=0;
    char* ptr[10];

    printf("\nBegin test at heap end %p:\n", sbrk(0));   
    my_mallinfo();
    b = 10;
    printf("Mallocing %d bytes:\n", b);
    data = (char*) my_malloc(10);
    if(data == NULL) {
	printf("%s", my_malloc_error);
  	errors++;
    }
    printf("\nTotal error count = %d\n", errors);
 
    printf("Should have %d bytes allocated, %d bytes free space:\n", b, f);
    my_mallinfo();
    printf("\nHeap now ends at %p\n", sbrk(0));
    
    for(i = 0; i < 10; i++) {
        data[i] = i;
    }
    //sprintf(data, "datatest 1");
    char *tmp = "datatest 1";
    memcpy(data, tmp, 10);

    printf("Output from %p should read \"%s\":  ", data, tmp);
    for(i = 0; i < 10; i++) {
	printf("%c", data[i]);
        if(data[i] != *(tmp+i))
            errors++;
    }

    printf("\nTotal error count = %d\n", errors);
    
    printf("Output from %p should read \"%s\":  ", data, tmp);
    for(i = 0; i < 10; i++) {
	printf("%c", data[i]);
        if(data[i] != *(tmp+i))
            errors++;
    }
    printf("\nFreeing just-allocated data at address %p...\n", data);
    if (my_free(data) < 0) {
	printf("%s", my_malloc_error);
	errors++;
    }
    printf("Output from %p should read \"%s\":  ", data, tmp);
    for(i = 0; i < 10; i++) {
	printf("%c", data[i]);
        if(data[i] != *(tmp+i))
            errors++;
    }
    b = 0;
    f = 10;
    printf("Should have %d bytes allocated, %d bytes free space:\n", b, f);
    my_mallinfo();
    printf("Total error count = %d\n\n", errors);


    for(i = 0; i < 10; i++) {
        ptr[i] = my_malloc((10-i)*10);
        if(ptr == NULL) printf("%s", my_malloc_error);
    }
    
    printf("\nAfter Allocating Pointers\n");
    my_mallinfo();
    
    my_free(ptr[0]);
    my_free(ptr[1]);
    
    printf("\nAfter Freeing First two Pointers (Merge Test)\n");
    my_mallinfo();
    
    my_free(ptr[3]);
    my_free(ptr[9]);
    
    printf("\nAfter Freeing two Pointers\n");
    my_mallinfo();
    
    ptr[0] = (char*) my_malloc(10);
    
    printf("\nAfter mallocing (First Fit test)\n");
    my_mallinfo();
    
    my_free(ptr[7]);
    printf("\nAfter freeing\n");
    my_mallinfo();
    
    my_mallopt(BEST_FIT);
    
    ptr[1] = (char*) my_malloc(10);
    printf("\nAfter mallocing again(Best Fit Test)\n");
    my_mallinfo();
    
    ptr[3] = my_malloc(136);
    printf("\nAfter mallocing big block\n");
    my_mallinfo();
    
    my_free(ptr[3]);
    printf("\nAfter freeing last block (Decrease Heap Test)\n");
    my_mallinfo();
    
    printf("\n--- DONE (%d errors) --- \n", errors);

    return 0;
}


/* API FUNCTIONS ---------------------------------------------------------------------------------------------------- */

/* returns void pointer that can assign to any C pointer
 * if memory cannot be allocated, returns NULL and sets global error string var */
void *my_malloc(int size)
{
    frblk_t *curr_block = frblk_list;
    frblk_t *frblk = NULL;
    tag_t* itag;

    // first scan list of free memory blocks previously released by my_free() to find one whose size is >= one to be allocated
    if (alloc_policy == FIRST_FIT)      // first fit allocation - allocate first block large enough
        while (curr_block != NULL) {
            if (curr_block->tag.length >= size) {
                frblk = curr_block;
                break;
            }
            curr_block = curr_block->next;
        }
    else                                // best fit allocation - allocate smallest block large enough
        while (curr_block != NULL) {
            if (curr_block->tag.length >= size) {
                if ((frblk == NULL) || (curr_block->tag.length < frblk->tag.length))
                    frblk = curr_block;
            }
	    curr_block = curr_block->next;
        }

    // remove free block from free block linked list if a previously freed block
    if (frblk != NULL) {
	tag_t* t = &(frblk->tag);
	printf("coolio! %p, %s, %p\n", (void*)&t[1], (char*)&t[1], sbrk(0));
	/* to reduce internal fragmentation:
	 * if frblk is large enough to hold the size of requested bytes + 10 bytes + 2 sets of end/start tags
	 * (size of one set is already accounted for in the free block), split the block into an allocated block
	 * of the right size, plus the excess free block (and put onto linked list) */
	if ((int)(frblk->tag.length) >= (int)(size + MIN_EXCESS_SPLIT + 2*sizeof(tag_t))) {
	    // set tags of new free block
	    tag_t* next_tag = get_end_tag(&(frblk->tag)) + 1;
	    next_tag->length = frblk->tag.length - size - 2*sizeof(tag_t);
	    set_end_tag(next_tag);
	    // put new free block into free block linked list
	    frblk_t* frblk_new = (frblk_t*)((void*)next_tag);
	    insert_to_free_list(frblk_new);
	}

	frblk_t *p, *n;
        if (frblk == frblk_list) {
            frblk_list = frblk_list->next;
            frblk_list->prev = NULL;
        }
        else {
            p = frblk->prev;
            n = frblk->next;
            p->next = n;
            n->prev = p;
        }
        itag = &(frblk->tag);
        total_free_bytes -= frblk->tag.length;
    }

    // no block in free list is large enough, allocate more memory
    else {
        itag = sbrk(2*sizeof(tag_t) + size);     // size of both tags + requested data
        if (itag == (void*)-1) {
            strcpy(my_malloc_error, "No more allocatable memory.\n");
            return NULL;
        }
    }

    itag->length = size;        // does not include size of tags
    itag->stat = USED;
    set_end_tag(itag);
    total_alloc_bytes += itag->length;
    printf("Allocated %d bytes at address %p\n", size, (void*)&itag[1]);
    return (void*)&itag[1];     // returns pointer to data
}


/* deallocates block of memory pointed to by the ptr arg
 * ptr arg should be an address previous allocated by the malloc package
 * does now lower program break unless last free block is 128 kb or more; otherwise simply
 * adds block of freed memory to free list to be recycled by future calls to malloc() */
int my_free(void *ptr)
{
    // check if address is valid
    if ((ptr == NULL) || (frblk_list && ptr < ((void*)frblk_list + sizeof(tag_t))) || (ptr >= sbrk(0))) {
        strcpy(my_malloc_error, "Error: Null or invalid pointer; nothing freed.\n");
        return -1;
    }

    /* check adjacent blocks for their status; if free blocks are next to the block being freed, merge into larger block */
    frblk_t *frblk = (frblk_t *) ((char *) ptr - sizeof(tag_t));    // ptr points to data, decrement to beginning of block

    total_alloc_bytes -= frblk->tag.length;
    insert_to_free_list(frblk);

    return 0;
}


/* specifies malloc policy */
void my_mallopt(int policy)
{
    alloc_policy = policy;
}


/* prints stats about malloc performed so far */
void my_mallinfo()
{
    int cont_free_space = 0;
    frblk_t* frblk_it = frblk_list;
    while (frblk_it != NULL)
    {
        cont_free_space = (frblk_it->tag.length < cont_free_space) ? frblk_it->tag.length : cont_free_space;
        frblk_it = frblk_it->next;
    }

    char buf[200];
    sprintf(buf, "Total memory allocated: %d bytes\nTotal free space: %d bytes\nLargest contiguous free space: %d bytes\n", total_alloc_bytes, total_free_bytes, cont_free_space);
    puts(buf);

}


/* HELPER FUNCTIONS ------------------------------------------------------------------------------------------------- */


/* sets the end tag to be the same as the start tag */
void set_end_tag(tag_t* itag)
{
    tag_t* ftag = get_end_tag(itag);
    ftag->length = itag->length;
    ftag->stat = itag->stat;
}


/* as we do not allocate a data section within a block, must write the end tag as if part of the data,
 * and retrieve it as such */
tag_t* get_end_tag(tag_t* itag)
{
    char* data = (char *)&itag[1];
    tag_t *ftag = (tag_t*)&data[itag->length];
    return ftag;
}


/* insert free block into free block list
 * also merges if the free block to insert is adjacent to any other free blocks
 * and reduces size of program break if the last free block is >= 128 kb */
int insert_to_free_list(frblk_t* to_in) {
    // insert free blocks into free block linked list
    if (frblk_list == NULL) {
	to_in->next = NULL;
        frblk_list = to_in;
        frblk_list->prev = NULL;
        frblk_list->next = NULL;
    }
    else if (to_in < frblk_list) {
        frblk_list->prev = to_in;
        to_in->prev = NULL;
        to_in->next = frblk_list;
        frblk_list = to_in;
    }
    else {
        frblk_t *curr_block = frblk_list;
        frblk_t *next_block;
        while (curr_block->next != NULL) {
            next_block = curr_block->next;
            if (to_in < next_block) {
                curr_block->next = to_in;
                next_block->prev = to_in;
                to_in->prev = curr_block;
                to_in->next = next_block;
                break;
            }
        }
        if (curr_block->next == NULL) {
            curr_block->next = to_in;
            to_in->prev = curr_block;
            to_in->next = NULL;
        }
    }

    to_in->tag.stat = FREE;
    set_end_tag(&(to_in->tag));
    total_free_bytes += to_in->tag.length;

    if ((void*)(to_in->next) == (void*)0xa0000)
	to_in->next = NULL;

    // merge free blocks
    tag_t *tag;
    if (to_in != frblk_list) {
        // get end tag of previous block
        tag = (tag_t *)to_in;
        tag--;                  // start tag of current block adjacent to end tag of previous block
        if (tag->stat == FREE)
            merge_blocks(to_in->prev, to_in);
    }

    if (to_in->next != NULL) {
        // get start tag of next block
        tag = get_end_tag(&(to_in->tag));
        tag++;                  // start tag of next block adjacent to end tag of current block;
        if (tag->stat == FREE)
            merge_blocks(to_in, to_in->next);
    }

    // should reduce program break if top free block is larger than 128 kb
    if ((to_in->next == NULL) && (to_in->tag.length >= LAST_BLOCK_MAX_BYTES)) {
        int redux = 0;
        char *last_heap_addr, *last_block_addr;
        tag_t* ftag;
        last_heap_addr = sbrk(0);
        if (last_heap_addr == (void*)-1) {
            strcpy(my_malloc_error, "Error: sbrk failed.\n");
            return -1;
        }

        ftag = get_end_tag(&(to_in->tag));
        last_block_addr = (char *)(&ftag[1]);

        // check for allocated data blocks following
        if (last_block_addr == last_heap_addr) {
            redux = to_in->tag.length;
            sbrk(-redux);
            to_in->tag.length -= redux;
            set_end_tag(&(to_in->tag));
        }

        total_free_bytes -= redux;
    }
}


/* merge two adjacent blocks */
void merge_blocks(frblk_t* first, frblk_t* second)
{
    printf("merging...\n");
    first->next = second->next;
    first->tag.length += (second->tag.length + 2*sizeof(tag_t));

    // gain two tags of free space from the merge
    total_free_bytes += 2*sizeof(tag_t);
    set_end_tag(&(first->tag));
    printf("merged!\n");

}
