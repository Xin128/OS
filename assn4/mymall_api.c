/* Author: Lara Goxhaj  - 260574574
 * COMP 310/ECSE 427
 * Malloc implementation API (with test included in main method)
 *     Supports first-fit and best-fit allocation
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "mymall_api.h"

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
char* base;

void set_end_tag(tag_t* itag);
tag_t* get_end_tag(tag_t* itag);
int insert_to_free_list(frblk_t* to_in);
void merge_blocks(frblk_t* first, frblk_t* second);


/* API FUNCTIONS ---------------------------------------------------------------------------------------------------- */

/* returns void pointer that can assign to any C pointer
 * if memory cannot be allocated, returns NULL and sets global error string var */
void *my_malloc(int size)
{
    if (size <= 0) {
        strcpy(my_malloc_error, "ERROR: Must allocate 1 or more bytes.\n");
        return NULL;
    }

    if (frblk_list == sbrk(0))
        frblk_list = NULL;
    frblk_t *curr_block = frblk_list, *frblk=NULL, *frblk_new, *nex;
    tag_t* itag;
    int reinsert=0, dec=0;

    if (total_alloc_bytes == 0 && total_free_bytes == 0)
	base = sbrk(0);

    // first scan list of free memory blocks previously released by my_free() to find one whose size is >= one to be allocated
    if (alloc_policy == FIRST_FIT) {     // first fit allocation - allocate first block large enough
        while (curr_block != NULL) {
            if (curr_block->tag.length >= size) {
                frblk = curr_block;
                break;
            }
            curr_block = curr_block->next;
        }
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
	/* to reduce internal fragmentation:
	 * if frblk is large enough to hold the size of requested bytes + 10 bytes + 2 sets of end/start tags
	 * (size of one set is already accounted for in the free block), split the block into an allocated block
	 * of the right size, plus the excess free block (and put onto linked list) */
	if ((int)(frblk->tag.length) >= (int)(size + MIN_EXCESS_SPLIT + 2*sizeof(tag_t))) {
	    nex = frblk->next;		// for later resetting
	    dec = frblk->tag.length;
	    // reset length and status of new used block
	    itag = &(frblk->tag);
	    itag->length = size;
	    itag->stat = USED;
	    set_end_tag(itag);
      printf("get end: %p, %p\n", itag, get_end_tag(itag));
	    // set tags of new free block
	    tag_t* next_tag = get_end_tag(itag) + 1;
	    next_tag->length = dec - size - 2*sizeof(tag_t);
	    set_end_tag(next_tag);
	    // put new free block into free block linked list
	    frblk_new = (frblk_t*)((void*)next_tag);
	    frblk_new->next = frblk->next;
	    reinsert = 1;
	}

	frblk_t *p, *n;
        if (frblk == frblk_list) {
	    // memory address sometimes reset to something beyond normal memory bounds
	    if ((void*)(frblk_list->next) > (void*)0x1000000000000)
		frblk_list = nex;
	    else
	        frblk_list = frblk_list->next;
	    if (frblk_list != NULL)
		frblk_list->prev = NULL;
        }
        else {
            p = frblk->prev;
            n = nex;
            p->next = n;
	    if (n != NULL)
        	n->prev = p;
        }
        itag = &(frblk->tag);
        total_free_bytes -= frblk->tag.length;

	if (reinsert) {
    	    if (insert_to_free_list(frblk_new) < -1)
		return NULL;
	    else
		total_free_bytes -= (dec - frblk->tag.length);
	}

    }

    // no block in free list is large enough, allocate more memory
    else {
        itag = sbrk(2*sizeof(tag_t) + size);     // size of both tags + requested data
        if (itag == (void*)-1) {
            strcpy(my_malloc_error, "ERROR: No more allocatable memory.\n");
            return NULL;
        }
    }

    itag->length = size;        // does not include size of tags
    itag->stat = USED;
    set_end_tag(itag);
    total_alloc_bytes += itag->length;

    char buf[100];
    sprintf(buf, "Allocated %d bytes at address %p", size, (void*)&itag[1]);
    puts(buf);
    return (void*)&itag[1];     // returns pointer to data
}


/* deallocates block of memory pointed to by the ptr arg
 * ptr arg should be an address previous allocated by the malloc package
 * does now lower program break unless last free block is 128 kb or more; otherwise simply
 * adds block of freed memory to free list to potentially be recycled by future calls to malloc() */
int my_free(void *ptr)
{
    // check if address is valid in general
    if ((ptr == NULL) || (frblk_list && ptr < ((void*)base + sizeof(tag_t))) || (ptr >= sbrk(0))) {
        strcpy(my_malloc_error, "ERROR: Null or invalid pointer; nothing freed.\n");
        return -1;
    }

    frblk_t *frblk = (frblk_t *) ((char *) ptr - sizeof(tag_t));    // ptr points to data, decrement to beginning of block
    total_alloc_bytes -= frblk->tag.length;
    if (insert_to_free_list(frblk) < -1)
	return -1;

    return 0;
}


/* specifies malloc policy */
void my_mallopt(int policy)
{
    alloc_policy = policy;
}


/* prints stats about malloc api operations performed so far */
void my_mallinfo()
{
    int cont_free_space = 0;
    frblk_t* frblk_it = frblk_list;
    while (frblk_it != NULL) {// && (void*)frblk_it != base) {
        cont_free_space = (frblk_it->tag.length > cont_free_space) ? frblk_it->tag.length : cont_free_space;
        frblk_it = frblk_it->next;
    }

    char buf[200];
    sprintf(buf, "\n----- MYMALL INFO -----\nTotal memory allocated: %d bytes\nTotal free space: %d bytes\nLargest contiguous free space: %d bytes\n-----------------------\n", total_alloc_bytes, total_free_bytes, cont_free_space);
    puts(buf);
    return;
}


/* HELPER FUNCTIONS ------------------------------------------------------------------------------------------------- */


/* sets the end tag to be the same as the start tag */
void set_end_tag(tag_t* itag)
{
    frblk_t *g = (frblk_t*)itag;
    frblk_t *tmp = g->next;       // somtimes resets, so reset manually
    tag_t* ftag = get_end_tag(itag);
    ftag->length = itag->length;
    g->next = tmp;
    ftag->stat = itag->stat;
}


/* as we do not allocate a data section within a block,
 * must write the end tag as if part of the data, and retrieve it as such */
tag_t* get_end_tag(tag_t* itag)
{
    char* data = (char *)&itag[1];
    tag_t *ftag = (tag_t*)&data[itag->length];
    return ftag;
}


/* insert free block into free block list, maintaining consecutive order of addresses
 * also merges if the free block to insert is adjacent to any other free blocks
 * and reduces size of program break if the last free block is >= LAST_BLOCK_MAX_BYTES */
int insert_to_free_list(frblk_t* to_in) {
    int merge_flag = 0;

    // insert free blocks into free block linked list
    if (frblk_list == NULL) {
	to_in->next = NULL;
        frblk_list = to_in;
        frblk_list->prev = 0;
        frblk_list->next = 0;
    }
    else if (to_in < frblk_list) {
        frblk_list->prev = to_in;
        to_in->prev = 0;
        to_in->next = frblk_list;
        frblk_list = to_in;
    }
    else {
        frblk_t *curr_block = frblk_list;
        frblk_t *next_block;
	int i = 0;
        while (curr_block->next != NULL) {
            next_block = curr_block->next;
	    i++;
            if (to_in < next_block && to_in > curr_block) {
                curr_block->next = to_in;
                next_block->prev = to_in;
                to_in->prev = curr_block;
                to_in->next = next_block;
                break;
            }
	    curr_block = next_block;
        }
        if (curr_block->next == NULL) {
            curr_block->next = to_in;
            to_in->prev = curr_block;
            to_in->next = 0;
        }
    }

    to_in->tag.stat = FREE;
    set_end_tag(&(to_in->tag));
    total_free_bytes += to_in->tag.length;

    // check adjacent blocks for their status; if free blocks are next to the block being freed, merge into larger block
    tag_t *tag;
    if (to_in != frblk_list) {
        // get end tag of previous block
        tag = (tag_t *)to_in;
        tag--;                  // start tag of current block adjacent to end tag of previous block
        if (tag->stat == FREE) {
            merge_blocks(to_in->prev, to_in);
            to_in = to_in->prev;
            merge_flag = 1;
        }
    }

    if (to_in->next != NULL) {
        // get start tag of next block
        tag = get_end_tag(&(to_in->tag));
        tag++;                  // start tag of next block adjacent to end tag of current block;
        if (tag->stat == FREE) {
            merge_blocks(to_in, to_in->next);
            merge_flag = 1;
        }
    }

    // should reduce program break if top free block is larger than LAST_BLOCK_MAX_BYTES
    if ((to_in->next == NULL) && (to_in->tag.length >= LAST_BLOCK_MAX_BYTES)) {
        int redux = 0;
        char *last_heap_addr, *last_block_addr;
        tag_t* ftag;
        last_heap_addr = sbrk(0);
        if (last_heap_addr == (void*)-1) {
            strcpy(my_malloc_error, "ERROR: sbrk failure; insertion into free list failed.\n");
            return -1;
        }

        ftag = get_end_tag(&(to_in->tag));
        last_block_addr = (char *)(&ftag[1]);

        // check for allocated data blocks following
        if (last_block_addr == last_heap_addr) {
            redux = to_in->tag.length + 2*sizeof(tag_t);
            to_in->tag.length -= redux;
            if (to_in->tag.length == -16)
                to_in->tag.length += 16;
            set_end_tag(&(to_in->tag));
            sbrk(-redux);
            total_free_bytes += 16;
        }

        total_free_bytes -= redux;
        if (total_free_bytes == 0)
            frblk_list = NULL;
    }

    return 0;
}


/* merge two adjacent blocks
 * note that first and second blocks should be in their respective positions in memory */
void merge_blocks(frblk_t* first, frblk_t* second)
{
    char start[100];
    sprintf(start, "Merging free blocks at %p, %p...", ((void*)first+sizeof(tag_t)), ((void*)second+sizeof(tag_t)));
    puts(start);
    first->next = second->next;
    first->tag.length += (2*sizeof(tag_t) + second->tag.length);
    total_free_bytes += 2*sizeof(tag_t);  // gain two tags of free space from the merge
    set_end_tag(&(first->tag));
    second->tag.length = first->tag.length;
    char end[10] = "Merged!";
    puts(end);
    return;
}
