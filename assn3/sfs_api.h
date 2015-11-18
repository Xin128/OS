/* DEFINES ---------------------------------------------------------------------------------------------------------- */

#define MAX_FILES 			    100                             // arbitrary
#define MAX_OPEN_FILES		  32	// 10
#define MAXFILENAME			    16
#define MAXEXT				      3
#define DIRECT_PTRS     	  12
#define PTR_SIZE			      4
#define MAX_FILE_SIZE   	  BLOCK_SIZE*(DIRECT_PTRS + BLOCK_SIZE/PTR_SIZE)  // 12 direct ptrs (12 blocks) + 1 block of indirect ptrs (512/4 blocks)
#define INODE_PER_BLOCK		  7                               // floor((double)BLOCK_SIZE/sizeof(inode_t))
#define DENTRY_PER_BLOCK    21                              // floor((double)BLOCK_SIZE/sizeof(dir_entry_t))

#define SFS_DISK			      "sfs_disk.img"
#define MAGIC_NUM       	  0xAABB0005
#define BLOCK_SIZE 			    512
#define TOTAL_BLOCKS        2000
#define ROOT_NUM			      0				                // for root dir inode and fd
#define SBLOCK_ADDR			    0
#define INODE_START_ADDR    SBLOCK_ADDR + 1                 // 1
#define INODE_TBL_LEN       15                              // ceil((double)(sizeof(inode_t)*MAX_FILES)/BLOCK_SIZE)
#define DIR_START_ADDR      INODE_START_ADDR + INODE_TBL_LEN    // 1 + 15 = 16
#define DIR_SIZE            5                               // ceil((double)(MAX_FILES * sizeof(dir_entry_t)/BLOCK_SIZE)
#define DATA_START_ADDR     DIR_START_ADDR + DIR_SIZE       // 16 + 5 = 21
#define DATA_BITMAP_SIZE    4                               // ceil((double)(DATA_BLOCKS)/BLOCK_SIZE)
#define DATA_BLOCKS         TOTAL_BLOCKS - DATA_START_ADDR - DATA_BITMAP_SIZE  // 2000 - 21 - 4 = 1975
#define DATA_BITMAP_ADDR    TOTAL_BLOCKS - DATA_BITMAP_SIZE // 2000 - 4 = 1996

#define FREE            	  1
#define USED            	  0

#define min(A,B)            ( (A)>(B) ? (B):(A) )
#define max(A,B)            ( (A)>(B) ? (A):(B) )


/* STRUCTS ---------------------------------------------------------------------------------------------------------- */

typedef struct super_block_t {
    int magic;
    int block_size;
    int fs_size;
    int inode_table_len;
    int root_dir_inode;
} super_block_t;

typedef struct inode_t {                        // sizeof(inode_t) = 18*4 = 72
    int mode;
    int link_cnt;
    int uid;
    int gid;
    int size;
    int direct_ptrs[DIRECT_PTRS];      // point to block numbers
    int indirect_ptr;
} inode_t;

typedef struct dir_entry_t {                    // sizeof(dir_entry_t) = 24
    char name[MAXFILENAME+1+MAXEXT];
    int inode_idx;
} dir_entry_t;

typedef struct fd_t {
    char status;
    int inode_idx;
    int rw_ptr;
} fd_t;


/* API FUNCTIONS ---------------------------------------------------------------------------------------------------- */

void mksfs(int fresh);
int sfs_getnextfilename(char *fname);
int sfs_getfilesize(const char* path);
int sfs_fopen(char *name);
int sfs_fclose(int fileID);         // TODO: void?
int sfs_fread(int fileID, char *buf, int length);   // TODO: void?
int sfs_fwrite(int fileID, const char *buf, int length);    // TODO: void?
int sfs_fseek(int fileID, int loc);     // TODO: void?
int sfs_remove(char *file);


/* USER-DEFINED HELPER FUNCTIONS ------------------------------------------------------------------------------------ */

void write_inode_by_index(int inode_idx);
void write_dir_entry_by_index(int dir_idx);
void write_bitmap_by_index(int bit_idx);
int get_inode_index_by_name(const char* name);
int get_empty_block();
int allocate_empty_blocks(int inode_idx, int num_blocks_needed);
int get_block_ptr_offset(int inode_idx, int block_num);
