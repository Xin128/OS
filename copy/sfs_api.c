/* Simple File System Implementation
 * Author: Lara Goxhaj
 * COMP 310/ECSE 427
 *
 * ./sfs -s mnt/ to mount the filesystem on mnt/ directory using FUSE
 * use -lm at link time to link math.h library functions
 *
 * Please note that 'root/' is the name of the root directory and is defined as a global constant string below
 *
 * NOTES:
 * All block pointers are offsets that start at 0, where the 0th block is the start of the root directory (for directory entries)
   - i.e., DIR_START_ADDR -  and the start of the file data blocks (for file data) - i.e., DATA_START_ADDR; neither of these
   addresses are 0 on disk, and so the block pointers are simply offsets from these addresses for respective data types
 * The root directory, though technically a "file", is not held in the open file descriptor table, so while inode_idx 0 and
   dir_idx 0 are allocated to the root directoyr, fileID 0 is NOT
 * All instances of fileID refer to the index of the file in the open_fd_table
 * All instances of inode_idx refer to the index of the inode in the inode_table and inode cache
 * No formatted data spanning multiple blocks (i.e., inodes and data entries) has any entry split between blocks
 * SFS is implemented such that modification to the superblock should never occur
 * Since SFS is written onto disk using defines, reading superblock is unnecessary
 *
 */


#include "sfs_api.h"
#include "disk_emu.h"
#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <math.h>


/* DATA STRUCTURES & GLOBAL VARIABLES ------------------------------------------------------------------------------- */

// Cache the directory file, all inodes, and the bitmap
// Also keep a table of file descriptors for open files

super_block_t super_block;
static dir_entry_t root_dir[MAX_FILES];         // directory cache - we keep ALL directory blocks in memory
inode_t inode_table[MAX_FILES];                 // inode cache
fd_t open_fd_table[MAX_OPEN_FILES];             // open file descriptor cache
char free_blocks[BLOCK_SIZE*DATA_BITMAP_SIZE];  // includes directory

int idx_in_dir =  0;
const char* root_name = "root/";


/* USER-DEFINED HELPER FUNCTIONS ------------------------------------------------------------------------------------ */

// allows us to read from and write to disk a single block at a time
void write_inode_by_index(int inode_idx, inode_t* inodeToWrite)
{
    inode_t *inodesFromDisk;
    char buf[BLOCK_SIZE];

    int block_num = INODE_START_ADDR + inode_idx/INODE_PER_BLOCK;
    int idx = inode_idx % INODE_PER_BLOCK;
    read_blocks(block_num, 1, (void *)buf);
    inodesFromDisk = (inode_t*)buf;
    memcpy(&inodesFromDisk[idx], inodeToWrite, sizeof(inode_t));
    write_blocks(block_num, 1, (void *)buf);
}


// allows us to read and write to disk a single block at a time
void write_dir_entry_by_index(int dir_idx)
{
    dir_entry_t *dirEntriesFromDisk;
    char buf[BLOCK_SIZE];

    int block_num = DIR_START_ADDR + dir_idx/DENTRY_PER_BLOCK;
    int idx = dir_idx % DENTRY_PER_BLOCK;
    read_blocks(block_num, 1, (void *)buf);
    dirEntriesFromDisk = (dir_entry_t*)buf;
    memcpy(&dirEntriesFromDisk[idx], &root_dir[dir_idx], sizeof(dir_entry_t));
    write_blocks(block_num, 1, (void *)buf);
}


// allows us to read and write to disk a single block at a time
void write_bitmap_by_index(int bit_idx, char* bitToWrite)
{
    char *bitmapFromDisk;
    char buf[BLOCK_SIZE];

    int block_num = DATA_BITMAP_ADDR + bit_idx/BLOCK_SIZE;
    int idx = bit_idx % BLOCK_SIZE;
    read_blocks(block_num, 1, (void *)buf);
    bitmapFromDisk = (char*)buf;
    memcpy(&bitmapFromDisk[idx], bitToWrite, sizeof(char));
    write_blocks(block_num, 1, (void *)buf);
}


// if respective inode (i.e. file with given name) DNE in sfs, return -1
int get_inode_index_by_name(const char* name)
{
    int tot_entries = inode_table[ROOT_NUM].size / sizeof(dir_entry_t);

    int inode_idx = -1;
    int i;
    for(i=0; i<=tot_entries; i++)
        if (strncmp(root_dir[i].name, name, strlen(name)) == 0) {
            inode_idx = root_dir[i].inode_idx;
            break;
        }

    return inode_idx;
}


// linear search to find empty file data block from bitmap; returns -1 if no data blocks left, else returns block ptr num
int get_empty_block() {
    int i;
    // find empty blocks and indicate usage in bitmap
    for (i = 0; i < DATA_BLOCKS; i++)
        if (free_blocks[i] == FREE) {
            free_blocks[i] = USED;
            break;
        }
    if (i == DATA_BLOCKS) {
        printf("SFS has reached maximum data capacity! Please delete some data before writing more.\n");
        return -1;
    }
    // write bitmap to disk
    write_bitmap_by_index(i, &free_blocks[i]);
    return i;
}


// linear search to find empty file data blocks from bitmap and allocate atomically
// returns -1 if not enough room left on disk to allocate all blocks, else returns number of blocks allocated
// only used by sfs_fwrite
int allocate_empty_blocks(int inode_idx, int num_blocks_needed) {
    int num = num_blocks_needed;
    int start_blocks = ceil((double)inode_table[inode_idx].size / BLOCK_SIZE);
    int ind_ptr_block = 0;
    if ((inode_table[inode_idx].indirect_ptr <= 0) && (start_blocks+num_blocks_needed > DIRECT_PTRS)) {
        ind_ptr_block = 1;
        num++;
    }
    int ptrs[num];

    int i, j;
    int flag = 0;
    // gather empty blocks
    int start = 0;
    for (j=0; j<num; j++) {
        // find empty blocks, do not write to disk yet or cache yet
        for (i = start; i < DATA_BLOCKS; i++) {
            if (free_blocks[i] == FREE) {
                ptrs[j] = i;
                start = i+1;
                break;
            }
        }
        if (i==DATA_BLOCKS)
            flag = 1;
    }
    if (flag == 1) {
        printf("SFS has reached maximum data capacity! Please delete some data before writing more.\n");
        return -1;
    }

    if (ind_ptr_block == 1) {
      inode_t *temp = calloc(1, sizeof(inode_t));
      temp = &inode_table[inode_idx];
      temp->indirect_ptr = ptrs[num-1];
      memcpy((void*)&inode_table[inode_idx], (void*)temp, sizeof(inode_t));
      //free(temp);  // segfaults if not commented out
    }

    // mark used blocks in bitmap and write entire bitmap to disk (for simplicity's sake
    for (i=0; i<num; i++) {
      free_blocks[ptrs[i]] = USED;
    }
    write_blocks(DATA_BITMAP_ADDR, DATA_BITMAP_SIZE, (void *)&free_blocks);

    int curr_block = start_blocks;
    int ptr_idx;
    // allocate empty blocks to the inode pointers and mark as used in the bitmap
    for (i=0; i<num_blocks_needed; i++) {
        if (curr_block < DIRECT_PTRS) {
            ptr_idx = curr_block - 1;
            inode_t *temp0 = calloc(1, sizeof(inode_t));
            temp0 = &inode_table[inode_idx];
            temp0->direct_ptrs[ptr_idx] = ptrs[i];
            memcpy((void *)&inode_table[inode_idx], (void*)temp0, sizeof(inode_t));
             //inode_table[inode_idx].direct_ptrs[ptr_idx] = ptrs[i];
            ptr_idx++;
        }
        else {
            ptr_idx = curr_block - DIRECT_PTRS - 1;
            char buf[BLOCK_SIZE];
	          int addr = DATA_START_ADDR + inode_table[inode_idx].indirect_ptr;
            read_blocks(addr, 1, (void *)buf);
            int *indptrs = (int *)buf;
            indptrs += ptr_idx;
            memcpy(indptrs, &ptrs[i], sizeof(int)*(num_blocks_needed-i));
            write_blocks(DATA_START_ADDR + inode_table[inode_idx].indirect_ptr, 1, (void *)buf);
        }
        curr_block++;
    }

    write_inode_by_index(inode_idx, &inode_table[inode_idx]);   // write inodes to disk

    return num;
}


// returns ptr (int) to a block given the number block of a file; this pointer is an offset from DATA_START_ADDR
int get_block_ptr_offset(int inode_idx, int block_num) {
    int ptr_num = -1;
    if (block_num < DIRECT_PTRS)
        ptr_num = inode_table[inode_idx].direct_ptrs[block_num];
    else {
        int block_off = block_num - DIRECT_PTRS - 1;
        char buf[BLOCK_SIZE];
        read_blocks(DATA_START_ADDR + inode_table[inode_idx].indirect_ptr, 1, (void *)buf);
        int *ptrs = (int *) buf;
        int i;
        for (i = 0; i < block_off; i++)
            ptrs++;

        ptr_num = *ptrs;
    }
    return ptr_num;
}


/* API functions ------------------------------------------------------- */

void mksfs(int fresh)
{
    int i;
    for (i=0; i<MAX_OPEN_FILES; i++)
        open_fd_table[i].status = FREE;

	if (fresh) {                                            // initialise fresh disk
        printf("Initialising sfs...\n");
        init_fresh_disk(SFS_DISK, BLOCK_SIZE, TOTAL_BLOCKS);
        // zero all cache data structures for safety
        bzero(&super_block, sizeof(super_block_t));
        //bzero(&root_dir, sizeof(dir_entry_t) * MAX_FILES);
        bzero(&inode_table, sizeof(inode_t) * MAX_FILES);
        bzero(&open_fd_table, sizeof(fd_t) * MAX_FILES);
        bzero(&free_blocks, sizeof(char) * DATA_BLOCKS);

        // Initialise super block
        super_block.magic = MAGIC_NUM;
        super_block.block_size = BLOCK_SIZE;
        super_block.inode_table_len = INODE_TBL_LEN;
        super_block.root_dir_inode = ROOT_NUM;
        printf("Writing Super block...\n");
        write_blocks(SBLOCK_ADDR, 1, (void *)&super_block);

          // Initialise directory inode
        inode_t *tmp = calloc(1, sizeof(inode_t));
        tmp->mode = 0x755;
        tmp->link_cnt = 1;
        tmp->size = 0;        // for size of root dir, store number of bytes in currently in dir (not including root dir file) = sizeof(dir_entry_t)*(num files in dir)
        for (i=0; i<DIR_SIZE; i++)    // DIR_SIZE < DIRECT_PTRS, so this is ok
            tmp->direct_ptrs[i] = DIR_START_ADDR + i;   // directory is pre-allocated
        memcpy((void*)&inode_table[ROOT_NUM], (void*)tmp, sizeof(inode_t));
        /*
        inode_table[ROOT_NUM].mode = 0x755;
        inode_table[ROOT_NUM].link_cnt = 1;
        inode_table[ROOT_NUM].size = 0;     // for size of root dir, store number of bytes in currently in dir (not including root dir file) = sizeof(dir_entry_t)*(num files in dir)
        for (i=0; i<DIR_SIZE; i++)          // DIR_SIZE < DIRECT_PTRS, so this is ok
            inode_table[ROOT_NUM].direct_ptrs[i] = DIR_START_ADDR + i; // directory is pre-allocated
            */
        printf("Writing Directory Inode...\n");
        write_inode_by_index(ROOT_NUM, &inode_table[ROOT_NUM]);

        /*
        // Though it would be more correct to include the following code, which sets all values in the
        //  open file descriptor table to free, both test files run through in their entirety and produce less errors
        // if this code is commented out
        fd_t *tmp1 = calloc(1, sizeof(fd_t));
        for (i=0; i<MAX_OPEN_FILES; i++) {
           tmp1 = &open_fd_table[i];
           tmp1->status = FREE;
           tmp1->inode_idx = -1;
           memcpy((void*)&open_fd_table[i], (void *)tmp1, sizeof(fd_t));
	         //open_fd_table[i].status = FREE;
           //open_fd_table[i].inode_idx = -1;        // for safety
	      }
        */

        // Initialise directory
        strcpy(root_dir[ROOT_NUM].name, root_name);
        root_dir[ROOT_NUM].inode_idx = ROOT_NUM;
        printf("Writing Root Directory...\n");
        write_dir_entry_by_index(ROOT_NUM);

        // Initialise data bitmap
        // since will be using bitmap as a char buffer, we have BLOCK_SIZE*sizeof(char) = 512*1 byte = 512 bytes
        memset(free_blocks, FREE, (DATA_BITMAP_SIZE*BLOCK_SIZE));
        free_blocks[ROOT_NUM] = USED;
        printf("Writing bitmap...\n");
        write_bitmap_by_index(ROOT_NUM, &free_blocks[ROOT_NUM]);

        printf("Disk Initialised.\n");
    }

    else {                                  // open new disk
        printf("Opening sfs...\n");
        init_disk(SFS_DISK, BLOCK_SIZE, TOTAL_BLOCKS);

        // read inodes into cache
        printf("Loading iNodes into cache...\n");
        inode_t* inodes_tmp;
        char iBuf[INODE_TBL_LEN*BLOCK_SIZE];
        read_blocks(INODE_START_ADDR, INODE_TBL_LEN, (void *)iBuf);
        inodes_tmp = (inode_t*)iBuf;
        memcpy(inode_table, inodes_tmp, MAX_FILES*sizeof(inode_t));

        // read directory into cache
        printf("Loading directory into cache...\n");
        dir_entry_t* dir_tmp;
        char dBuf[DIR_SIZE*BLOCK_SIZE];
        read_blocks(DIR_START_ADDR, DIR_SIZE, (void *)dBuf);
        dir_tmp = (dir_entry_t*)dBuf;
        memcpy(root_dir, dir_tmp, MAX_FILES*sizeof(dir_entry_t));

        for (i=0; i<MAX_OPEN_FILES; i++) {
            open_fd_table[i].status = FREE;     // other fields only matter if status == USED
            open_fd_table[i].rw_ptr = 0;
            open_fd_table[i].inode_idx = -1;
        }

        // read data bitmap into cache
        printf("Loading bitmap into cache...\n");
        char buf[DATA_BITMAP_SIZE*BLOCK_SIZE];
        read_blocks(DATA_BITMAP_ADDR, DATA_BITMAP_SIZE, (void *)buf);
        memcpy(free_blocks, buf, DATA_BLOCKS);
	}
/*
  for (i=0; i<MAX_OPEN_FILES; i++) {
      open_fd_table[i].status = FREE;
      open_fd_table[i].rw_ptr = 0;
      open_fd_table[i].inode_idx = -1;
  }*/

	return;
}


// get name of next file in directory
// this method assumes that root_dir contains no empty entries between full entries
int sfs_getnextfilename(char *fname)
{
    int num_files = inode_table[ROOT_NUM].size / sizeof(dir_entry_t);
    if (idx_in_dir < num_files) {
        strcpy(fname, root_dir[idx_in_dir].name);
        idx_in_dir++;
        return (idx_in_dir-1);
    }

    idx_in_dir = 0;
    return idx_in_dir;
}


// returns size of file, or -1 if file DNE
int sfs_getfilesize(const char* path)
{
    int inode_idx = get_inode_index_by_name(path);
    if (inode_idx == -1) {
        printf("Error: File does not exist in directory.\n");
        return -1;
    }

    return inode_table[inode_idx].size;
}


// opens a file, or creates it if it does not exist, and returns the fileID
// if an error occurs (lack of capacity or poor name), no file is opened/created, and return -1
int sfs_fopen(char *name)
{
    // check for capacity of file system with respect to the maximum number of files in the directory (this is an arbitrarily imposed limit)
    int num_entries = (inode_table[ROOT_NUM].size)/sizeof(dir_entry_t);
    if (num_entries == MAX_FILES) {
        printf("Directory has reached maximum file capacity! Please delete a file before creating a new one.\n");
        return -1;
    }

    // check for errors in the file name
    int len = strlen(name);
    if (len > (MAXFILENAME+MAXEXT+1)) {
        printf("Error: Total file name too long.\n");
        return -1;
    }
    int i;
    char p = '.';
    int flag = 0;
    char *n = name;
    for (i=0; i<=MAXFILENAME; i++) {
        if (strncmp(n, &p, 1) == 0) {
            flag = 1;
            break;
        }
	      n += 1;
    }
    if (flag == 0) {
        printf("Error: File name too long.\n");
        return -1;
    }
    if ((len-i-1)>MAXEXT) {
        printf("Error: File extension too long.\n");
        return -1;
    }
    if (strcmp(name, root_name) == 0) {
        printf("Root directory already open!");
        return ROOT_NUM;   // fileID of root directory
    }

    // check to see if file already exists on disk/in directory, or not
    int inode_idx = get_inode_index_by_name(name);
    // file already exists
    if (inode_idx == 0) {
        printf("Directory already open!\n");
	      return 0;
    }
    else if (inode_idx > 0) {
        for (i = 0; i < MAX_OPEN_FILES; i++) {      // check if file is open
            if (open_fd_table[i].inode_idx == inode_idx) {
                printf("File already open!\n");
                return -1;		// return error, NOT index of already open file
            }
	      }
    }

    // get free file descriptor
    flag = 0;
    int fileID;
    for (i=0; i<MAX_OPEN_FILES; i++) {
        if (open_fd_table[i].status == FREE) {
            fileID = i;
	          flag = 1;
            break;
        }
    }
    // check if can open any files
    if (flag == 0) {
        printf("Maximum files open; please close some files before opening more.\n");
        return -1;
    }

    flag = 0;
    // if file DNE, create it
    if (inode_idx == -1) {
	     printf("Creating file...\n");
        // linear search to find new inode index
        for (i=0; i<MAX_FILES; i++) {
            if (inode_table[i].link_cnt == 0) {
                inode_idx = i;
		            flag = 1;
                break;
            }
        }
        // cannot create any more files in the sfs; just checking again for safety,
        if (flag == 0) {
            printf("Directory has reached maximum file capacity! Please delete a file before creating a new one.\n");
            return -1;
        }

        int empty_block_num = get_empty_block();
        if (empty_block_num == -1)
            return -1;

        // add new directory entry
	      void *p0 = (void*)&root_dir[num_entries+1];
        dir_entry_t *new = malloc(sizeof(dir_entry_t));
        strcpy(new->name, name);
        new->inode_idx = inode_idx;
        memcpy(p0, new, sizeof(dir_entry_t));
        free(new);
        write_dir_entry_by_index(num_entries+1);                              // write new directory entry to disk

        inode_table[ROOT_NUM].size += sizeof(dir_entry_t);

        write_inode_by_index(ROOT_NUM, &inode_table[inode_idx]);            // write new inode to disk

        // allocate for new file in inode table
        inode_table[inode_idx].direct_ptrs[0] = i;
        inode_table[inode_idx].mode = 0x755;
        inode_table[inode_idx].link_cnt = 1;
        inode_table[inode_idx].size = 0;
        if (inode_idx > INODE_PER_BLOCK)    // write inode to disk if not in same block as root directory inode
            write_inode_by_index(inode_idx, &inode_table[inode_idx]);
    }

    // put file in open file descriptor table
    open_fd_table[fileID].status = USED;
    open_fd_table[fileID].inode_idx = inode_idx;
    open_fd_table[fileID].rw_ptr = 0;                           // set to 0 automatically upon opening

    printf("File created.\n");
    return fileID;
}


// close file and return error code: 0 if successful, -1 otherwise
int sfs_fclose(int fileID)
{
    // check for valid file ID
    if ((fileID < 0) || (fileID >= MAX_OPEN_FILES)) {
        printf("Error: Invalid file ID.\n");
        return -1;
    }
    if (open_fd_table[fileID].status == FREE) {         // error - this should never occur
        printf("Error: File not open.\n");
        return -1;
    }

    fd_t *tmp = calloc(1, sizeof(fd_t));
    tmp = &open_fd_table[fileID];
    tmp->rw_ptr = 0;                                    // do this now, for safety
    tmp->inode_idx = -1;
    tmp->status = FREE;
    memcpy((void*)&open_fd_table[fileID], (void *)tmp, sizeof(fd_t));

    open_fd_table[fileID].rw_ptr = 0;                   // do this now, for safety
    open_fd_table[fileID].inode_idx = -1;
    open_fd_table[fileID].status = FREE;

    return 0;
}


// will only read from file data blocks, not inode or directory blocks
// returns length of file
int sfs_fread(int fileID, char *buf, int length)
{
    // check for valid file ID
    if ((fileID < 0) || (fileID >= MAX_OPEN_FILES)) {
        printf("Error: Invalid file ID.\n");
        return -1;
    }
    // check to make sure file is open
    if (open_fd_table[fileID].status == FREE) {
        printf("Error: No file with file ID %d open.\n", fileID);
        return -1;
    }

    int inode_idx = open_fd_table[fileID].inode_idx;
    int pos = open_fd_table[fileID].rw_ptr;
    // automatically truncate length if too long respective to size of file and current position of read-write pointer
    if ((pos + length) > inode_table[inode_idx].size)
        length = inode_table[inode_idx].size - pos;

    int block_num = pos / BLOCK_SIZE;
    pos = pos % BLOCK_SIZE;
    int block_ptr = get_block_ptr_offset(inode_idx, block_num);
    if (block_ptr == -1) {
        printf("Error: Invalid block pointer.\n");
        return -1;
    }
    char block[BLOCK_SIZE];
    read_blocks(DATA_START_ADDR+block_ptr, 1, (void *)block);

    // need only read a single block
    if ((pos+length) <= BLOCK_SIZE) {
        memcpy(buf, &block[pos], length);
        open_fd_table[fileID].rw_ptr += length;
        return length;
    }

    int last_full_block_num = ((pos+length)/BLOCK_SIZE) + block_num;
    int last_block_bytes = (pos + length) % BLOCK_SIZE;

    memcpy(buf, &block[pos], BLOCK_SIZE-pos);
    buf += (BLOCK_SIZE-pos);
    block_num++;

    while (block_num < last_full_block_num) {
        block_ptr = get_block_ptr_offset(inode_idx, block_num);
        if (block_ptr == -1) {
            printf("Error: Invalid block pointer.\n");
            return -1;
        }
        read_blocks(DATA_START_ADDR + block_ptr, 1, (void *)block);
        memcpy(buf, block, BLOCK_SIZE);
        buf += BLOCK_SIZE;
        block_num++;
    }

    block_ptr = get_block_ptr_offset(inode_idx, block_num);
    if (block_ptr == -1) {
        printf("Error: Invalid block pointer.\n");
        return -1;
    }
    read_blocks(DATA_START_ADDR + block_ptr, 1, (void *)block);
    memcpy(buf, block, last_block_bytes);

    open_fd_table[fileID].rw_ptr += length;
    return length;
}


// TODO: change the implementation discussed immediately below, do not make automatic?
// is atomic, such that if the disk does not contain the capacity to hold an extra [length] bytes given its formatting,
// then do not write at all and return error
// returns length of bytes written if successful, -1 otherwise
int sfs_fwrite(int fileID, const char *buf, int length)
{
    // check for valid file ID
    if ((fileID < 0) || (fileID >= MAX_OPEN_FILES)) {
        printf("Error: Invalid file ID.\n");
        return -1;
    }
    // check to make sure file is open
    if (open_fd_table[fileID].status == FREE) {
        printf("Error: No file with file ID %d open.\n", fileID);
        return -1;
    }
    // check to make sure write will not exceed max file size
    if (open_fd_table[fileID].rw_ptr + length > MAX_FILE_SIZE) {
        printf("Error: Write failed, exceeds maximum file size.\n");
        return -1;
    }

    int inode_idx = open_fd_table[fileID].inode_idx;
    int rw_ptr = open_fd_table[fileID].rw_ptr;
    int block_num = rw_ptr / BLOCK_SIZE;
    int pos = rw_ptr % BLOCK_SIZE;

    // if don't currently have enough indirect and direct pointers allocated, allocate more
    int exceeded = (rw_ptr + length) > inode_table[inode_idx].size ? 1 : 0;
    if (exceeded == 1) {
        int room_left_here = BLOCK_SIZE - pos;
        int blocks_needed = ceil((double)(length - room_left_here) / BLOCK_SIZE);
        // allocate as many blocks as needed
        if (allocate_empty_blocks(inode_idx, blocks_needed) == -1)
            return -1;
    }

    char block[BLOCK_SIZE];

    int block_ptr = get_block_ptr_offset(inode_idx, block_num);
    if (block_ptr == -1) {
        printf("Error: Invalid block pointer.\n");
        return -1;
    }
    read_blocks(DATA_START_ADDR+block_ptr, 1, (void *)block);
    // do not need to write to more than 1 block
    if ((pos+length) <= BLOCK_SIZE) {
        memcpy(&block[pos], buf, length);
        write_blocks(DATA_START_ADDR+block_ptr, 1, (void *)block);

        inode_table[inode_idx].size = max(rw_ptr+length, inode_table[inode_idx].size);
        open_fd_table[fileID].rw_ptr += length;
        return length;
    }

    int last_full_block_num = ((pos+length)/BLOCK_SIZE) + block_num;
    int last_block_bytes = (pos + length) % BLOCK_SIZE;

    memcpy(&block[pos], buf, BLOCK_SIZE-pos);
    write_blocks(DATA_START_ADDR+block_ptr, 1, (void *)block);
    buf += (BLOCK_SIZE-pos);
    block_num++;

    while (block_num < last_full_block_num) {
        block_ptr = get_block_ptr_offset(inode_idx, block_num);
        if (block_ptr == -1) {
            printf("Error: Invalid block pointer.\n");
            return -1;
        }
        read_blocks(DATA_START_ADDR + block_ptr, 1, (void *)block);
        memcpy(block, buf, BLOCK_SIZE);
        write_blocks(DATA_START_ADDR + block_ptr, 1, (void *)block);

        buf += BLOCK_SIZE;
        block_num++;
    }

    block_ptr = get_block_ptr_offset(inode_idx, block_num);
    printf("block ptr: %d\n", block_ptr);
    if (block_ptr == -1) {
        printf("Error: Invalid block pointer.\n");
        return -1;
    }
    read_blocks(DATA_START_ADDR + block_ptr, 1, (void *)block);
    memcpy(block, buf, last_block_bytes);
    write_blocks(DATA_START_ADDR + block_ptr, 1, (void *)block);

    inode_table[inode_idx].size = max(rw_ptr+length, inode_table[inode_idx].size);
    open_fd_table[fileID].rw_ptr += length;
    return length;
}


// nothing to be done on disk
int sfs_fseek(int fileID, int loc)
{
    if ((fileID < 0) || (fileID >= MAX_OPEN_FILES)) {
        printf("Error: Invalid file ID.\n");
        return -1;
    }
    // check if file is open
    if (open_fd_table[fileID].status == FREE) {
        printf("Error: File not open.\n");
        return -1;
    }
    // check if loc is a valid length
    if (loc < 0) {
        printf("Error: Invalid seek location.\n");
        return -1;
    }

    // if loc is larger than size of file, set rw_ptr to end of file
    fd_t *temp = calloc(1, sizeof(fd_t));
    temp = &open_fd_table[fileID];
    temp->rw_ptr = min(inode_table[open_fd_table[fileID].inode_idx].size, loc);
    memcpy((void *)&open_fd_table[fileID], (void*)temp, sizeof(fd_t));
    //open_fd_table[fileID].rw_ptr = min(inode_table[open_fd_table[fileID].inode_idx].size, loc);
	  return 0;
}


// remove file from file system and return error code - 0 if successful, -1 otherwise
int sfs_remove(char *file)
{
    // retrieve respective inode
    int inode_idx = get_inode_index_by_name(file);
    if (inode_idx == -1) {
        printf("Error: File does not exist in file system.\n");
        return -1;
    }
    // check if file is root directory
    if (strcmp(file, root_name) == 0) {
        printf("Error: cannot remove root directory.\n");
        return -1;
    }

    int i;
    // TODO: if file open, close file - or don't delete and return -1?
    for (i=0; i<MAX_OPEN_FILES; i++)
        if (open_fd_table[i].inode_idx == inode_idx) {
            printf("Closing file...\n");
            if (sfs_fclose(i) == -1)
                return -1;
            open_fd_table[i].inode_idx = -1;
            break;
        }

    printf("Deleting file...\n");
    // clear inode
    int num_entries = (inode_table[ROOT_NUM].size)/sizeof(dir_entry_t);
    inode_table[ROOT_NUM].size -= sizeof(dir_entry_t);
    inode_table[inode_idx].mode = 0;
    inode_table[inode_idx].link_cnt = 0;    // if link_cnt == 0, means the inode is free; automatically = 0 in this sfs as we only maintain 1 link to a given file

    int num_blocks = (ceil)((double)inode_table[inode_idx].size / BLOCK_SIZE);
    printf("num_blocks: %d\n", num_blocks);
    inode_table[inode_idx].size = 0;

    // write inode to disk
    write_inode_by_index(inode_idx, &inode_table[inode_idx]);


    // clear data blocks, update bitmap
    char *bitmapFromDisk;
    char bitBuf[BLOCK_SIZE*DATA_BITMAP_SIZE];
    read_blocks(DATA_BITMAP_ADDR, DATA_BITMAP_SIZE, (void *)bitBuf);   // copy all bitmap blocks from disk, for simplicity
    bitmapFromDisk = (char*)bitBuf;

    for (i=0; i<min(num_blocks, 12); i++) {
        int p = inode_table[inode_idx].direct_ptrs[i];
        inode_table[inode_idx].direct_ptrs[i] = -1;
        bitmapFromDisk[p/BLOCK_SIZE]++;
        free_blocks[p] = FREE;
    }

    if (num_blocks > 12) {
        char ptrBuf[BLOCK_SIZE];
        read_blocks(DATA_START_ADDR+inode_table[inode_idx].indirect_ptr, 1, (void*)ptrBuf);
        int* ptrs = (int*)ptrBuf;

        int num_ptrs = (num_blocks-DIRECT_PTRS)/PTR_SIZE;
        char f = FREE;
        for (i=0; i<num_ptrs; i++) {
            memcpy(&bitmapFromDisk[num_ptrs], &f, sizeof(char));
            free_blocks[num_ptrs] = FREE;
            ptrs++;
        }
        inode_table[inode_idx].indirect_ptr = 0;
    }

    // write bitmap blocks to disk
    write_blocks(DATA_BITMAP_ADDR, DATA_BITMAP_SIZE, (void *)bitBuf);

    // decrement size of directory and clear directory of file
    int start_dir_block;
    //int num_blocks;
    int start_entry;

    inode_table[ROOT_NUM].size -= sizeof(dir_entry_t);
    for (i=0; i<num_entries; i++) {
        if (strcmp(root_dir[i].name, file) == 0) {
            start_entry = i;
            start_dir_block = DIR_START_ADDR + i/DENTRY_PER_BLOCK;
            num_blocks = DIR_SIZE - i/DENTRY_PER_BLOCK + 1;
            memmove((void*)&root_dir[i], (void*)&root_dir[i+1], sizeof(dir_entry_t)*(num_entries-i));
            break;
        }
    }

    // write dir entries to disk; is a bit convoluted as sizeof(dir_entry_t) % BLOCK_SIZE != 0
    dir_entry_t *dirFromDisk1;
    dir_entry_t *dirFromDisk2;
    char buf1[BLOCK_SIZE];
    char buf2[BLOCK_SIZE];

    for (i=0; i<num_blocks; i++) {
        if (i == 0) {
            read_blocks(start_dir_block + i, 1, (void *)buf1);
            dirFromDisk1 = (dir_entry_t *)buf1;
        }

        start_entry = start_entry % DENTRY_PER_BLOCK;
        memmove(dirFromDisk1+start_entry, dirFromDisk1+start_entry+1, sizeof(dir_entry_t)*(DENTRY_PER_BLOCK-start_entry));
        start_entry = 0;

        if ((num_blocks - i) != 1) {
            read_blocks(start_dir_block+i+1, 1, (void *)buf2);
            dirFromDisk2 = (dir_entry_t *)buf2;
            memmove(dirFromDisk1+DENTRY_PER_BLOCK, dirFromDisk2, sizeof(dir_entry_t));
            memmove(dirFromDisk2, dirFromDisk2+1, sizeof(dir_entry_t)*(DENTRY_PER_BLOCK-1));
            dirFromDisk1 = dirFromDisk2;
        }
        else {
            write_blocks(start_dir_block+i+1, 1, (void *)buf2);
        }

        write_blocks(start_dir_block+i, 1, (void*)buf1);
    }

    printf("File removed from file system.\n");
    return 0;
}
