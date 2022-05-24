#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>
#include <signal.h>
#include "ext2_fs.h"


#define SUPERBLOCK_OFFSET 1024
#define I_BLOCK_COUNT 15
struct ext2_super_block super_block;
struct ext2_group_desc* myGroups;
__u32 block_size;
int num_groups = 1;
int fd_image;

void superblock_summary();
void group_summary();
void block_bitmap(int num);
void inode_bitmap(int num);
void inode_table(int num);
void get_time( char* buf, time_t raw_time);
void indirect_block(__u32 block_number, int inode_number, int offset, int level, char file_type,struct ext2_inode* inode);
void direct_block(int reg,struct ext2_inode* inode, int inode_number, int logical_offset);
char get_file_type(__u16 i_mode);

//========================================================
void inode_table(int num)
{
    __u32 inode_table_index = myGroups[num].bg_inode_table;
    unsigned int inode_count = super_block.s_inodes_per_group;
    /* number of inodes per block */
    // unsigned int inodes_per_block = block_size / sizeof(struct ext2_inode);
    /* size in blocks of the inode table */
    // unsigned int itable_blocks = inode_count / inodes_per_block;

    size_t inode_table_size = inode_count*sizeof(struct ext2_inode);
    struct ext2_inode* inode_table;
    inode_table = (struct ext2_inode*) malloc(inode_table_size);

    if (pread(fd_image, inode_table, inode_table_size, inode_table_index*(int) block_size) < 0)
    {
      fprintf(stderr, "Error Occured when reading from the inode table\n");
      exit(1);
    }

    unsigned int i =0;
    for (; i < inode_count; i++)
    {
        struct ext2_inode inode_curr = inode_table[i];
        int inode_num =  num * inode_count + i + 1;
        if (inode_curr.i_mode != 0 && inode_curr.i_links_count != 0)
        {
            char file_type = get_file_type(inode_curr.i_mode);
            __u16 mask = 0xFFF;
            __u16 mode = inode_curr.i_mode & mask;
            char ctime_str[30];
            char mtime_str[30];
            char atime_str[30];
            get_time(ctime_str, inode_curr.i_ctime);
            get_time(mtime_str, inode_curr.i_mtime);
            get_time(atime_str, inode_curr.i_atime);

            printf("INODE,%u,%c,%o,%u,%u,%u,%s,%s,%s,%u,%u",
                   inode_num,
                   file_type,
                   mode,
                   inode_curr.i_uid,
                   inode_curr.i_gid,
                   inode_curr.i_links_count,
                   ctime_str,
                   mtime_str,
                   atime_str,
                   inode_curr.i_size,
                   inode_curr.i_blocks);
        

            unsigned int j =0;
            for (j = 0; j < I_BLOCK_COUNT; j++)
            {
                __u32 block_num = inode_curr.i_block[j];
                printf(",%u",block_num);
            }
            printf("\n");

            if (file_type == 'd')
            {
                direct_block(0,&inode_curr, inode_num,0);
            }
            // this will count how many pointers can be store in each block_pointers_block 
            __u32 block_ptr_count  = block_size / sizeof(u_int32_t);
            // first indirect  level
            if (inode_curr.i_block[EXT2_IND_BLOCK] !=0)
            {
                indirect_block(inode_curr.i_block[EXT2_IND_BLOCK] ,inode_num,12,1,file_type, &inode_curr);
            }
            // doubly indirect level
            int second_offset_start= 12+block_ptr_count;
            if (inode_curr.i_block[EXT2_DIND_BLOCK]!=0)
            {
                indirect_block(inode_curr.i_block[EXT2_DIND_BLOCK],inode_num,second_offset_start,2,file_type, &inode_curr);
            }
            // triple indirect level
            if (inode_curr.i_block[EXT2_TIND_BLOCK]!=0)
            {
                indirect_block(inode_curr.i_block[EXT2_TIND_BLOCK],inode_num,second_offset_start+block_ptr_count*block_ptr_count,3, file_type, &inode_curr);
            }
        }
    }
}


void get_time(char* buf, time_t raw_time)
{
	struct tm ts = *gmtime(&raw_time);
	strftime(buf, 80, "%m/%d/%y %H:%M:%S", &ts);
}


char get_file_type(__u16 i_mode)
{
    char type;

    if (S_ISDIR(i_mode))
        type = 'd';
    else if (S_ISREG(i_mode))
        type = 'f';
    else if (S_ISLNK(i_mode))
        type = 'l';
    else
        type = '?';

    return type;
}



void direct_block(int reg, struct ext2_inode* inode, int inode_number, int logical_offset)
{

    struct ext2_dir_entry *dir_entry = malloc(sizeof(struct ext2_dir_entry));
    unsigned i;
    if(reg)
    {
        unsigned int block_start = 0;
        while((unsigned) block_start < block_size)
        {
                    memset(dir_entry->name, 0, 256);
                    if (pread(fd_image, dir_entry, sizeof(struct ext2_dir_entry),   SUPERBLOCK_OFFSET+(logical_offset - 1) * block_size+block_start) < 0)
                    {
                    fprintf(stderr, "Error Occured when reading directory entries\n");
                    exit(1);
                    }
                    memset(&(dir_entry->name[dir_entry->name_len]), 0, 256 - dir_entry->name_len);

                    if(dir_entry->inode!=0)
                    {
                        printf("%s,%u,%d,%u,%u,%u,'%s'\n", "DIRENT",
                            inode_number,
                            block_start,
                            dir_entry->inode,
                            dir_entry->rec_len,
                            dir_entry->name_len,
                            dir_entry->name);
                    }
                    block_start += dir_entry->rec_len;
        }

    }else
    {
        for (i = 0; i < EXT2_NDIR_BLOCKS; i++)
        {
            if(inode->i_block[i]!=0)
            {
                while((unsigned) logical_offset < block_size)
                {
                    memset(dir_entry->name, 0, 256);
                    if (pread(fd_image, dir_entry, sizeof(struct ext2_dir_entry),  SUPERBLOCK_OFFSET+(inode->i_block[i]-1) * block_size + logical_offset) < 0)
                    {
                    fprintf(stderr, "Error Occured when reading directory entries\n");
                    exit(1);
                    }
                    memset(&(dir_entry->name[dir_entry->name_len]), 0, 256 - dir_entry->name_len);

                    if(dir_entry->inode!=0)
                    {
                        printf("%s,%u,%d,%u,%u,%u,'%s'\n", "DIRENT",
                            inode_number,
                            logical_offset,
                            dir_entry->inode,
                            dir_entry->rec_len,
                            dir_entry->name_len,
                            dir_entry->name);
                    }

                    logical_offset += dir_entry->rec_len;
                }
            }
        }



    }

  
}


/**
 * i_blocks
 * [0-11]---DIR----->[block data]                           
 * [bk12]---S_IND---->[ N_block_ptrs]->{[12][..][12+N-1]} 
 *                                     
 * 
 * 
 * 
 *                    [N_BLOCK_PTRS]->{[12+N][..][12+N+N-1] }
 * [bk13]---D_IND----<
 *                    [N_BLOCK_PTRS]->{[12+N+N].............}
 * 
 * 
 *                    [N_BLOCK_PTRS]->{[12+N^2][..][12+N^2+N-1] }
 * [bk14]---D_IND----<[N_BLOCK_PTRS]->{........................ }
 *                    [N_BLOCK_PTRS]->{........................[3*NNN]}
 * **/

void indirect_block(__u32 block_number, int inode_number, int offset, int level, char file_type,struct ext2_inode* inode)
{
    unsigned i;
    __u32 block_ptr_count  = block_size / sizeof(u_int32_t);
    __u32 *element = malloc(block_size * sizeof(__u32));

    if (pread(fd_image, element, block_size, block_size*block_number) < 0)
    {
      fprintf(stderr, "Error Occured reading from the indirect block\n");
      exit(1);
    }

    for (i = 0;i < block_ptr_count; i++)
    {
        if(element[i] != 0)
        {
            if (file_type == 'd'&& level==1)
                {
                    direct_block(1,inode,inode_number, element[i]);
                }
            printf("%s,%d,%d,%d,%u,%u\n", "INDIRECT",
                   inode_number,
                   level,
                   offset,
                   block_number,
                   element[i]);
            // use recursion to traverse
            if (level==2 || level==3)
            {
                indirect_block(element[i], inode_number, offset, level-1,file_type,inode);
            }
        }
        switch (level)
        {
          case 1:
            offset++;
            break;
          case 2:
            offset += block_ptr_count;
            break;
          case 3:
            offset += block_ptr_count * block_ptr_count;
            break;
        }
    }
}
/*
SUPERBLOCK
total number of blocks (decimal)
total number of i-nodes (decimal)
block size (in bytes, decimal)
i-node size (in bytes, decimal)
blocks per group (decimal)
i-nodes per group (decimal)
first non-reserved i-node (decimal)
*/
void superblock_summary()
{
    printf("%s,%u,%u,%u,%u,%u,%u,%u\n", "SUPERBLOCK",
           super_block.s_blocks_count,
           super_block.s_inodes_count,
           block_size,
           super_block.s_inode_size,
           super_block.s_blocks_per_group,
           super_block.s_inodes_per_group,
           super_block.s_first_ino);
}

//========================================================
/*
GROUP
group number (decimal, starting from zero)
total number of blocks in this group (decimal)
total number of i-nodes in this group (decimal)
number of free blocks (decimal)
number of free i-nodes (decimal)
block number of free block bitmap for this group (decimal)
block number of free i-node bitmap for this group (decimal)
block number of first block of i-nodes in this group (decimal)

*/

void group_summary()
{
    // fisrt calculate the numer of group on the disk
    if(pread(fd_image, myGroups,sizeof(struct ext2_group_desc), 1024+block_size) < 0)
    {
      fprintf(stderr, "Error Occured when calling pread for gd\n");
      exit(1);
    }

    // process each group on the disk
    int gp =0;
    int blocks_per_group = super_block.s_blocks_per_group;
    int lastBlockCount= super_block.s_blocks_count % blocks_per_group;
    int inodes_per_group = super_block.s_inodes_per_group;    /* # Inodes per group */
    int lastInodeCount = super_block.s_inodes_count % inodes_per_group;
    for( ; gp < num_groups ; gp++)
    {
        if( gp == num_groups - 1 )
        {
            if(lastBlockCount)
                blocks_per_group = lastBlockCount;
            if(lastInodeCount)
                inodes_per_group = lastInodeCount;
        }

        fprintf(stdout, "GROUP,%d,%d,%d,%d,%d,%d,%d,%d\n",
                gp,
                blocks_per_group,
                inodes_per_group,
                myGroups[gp].bg_free_blocks_count,
                myGroups[gp].bg_free_inodes_count,
                myGroups[gp].bg_block_bitmap,
                myGroups[gp].bg_inode_bitmap,
                myGroups[gp].bg_inode_table
                );
    }
}

//========================================================

/*EXT2
----------------------------------------------------
| block gp 0 | block gp 1 | block gp 2 |block gp 3 |
----------------------------------------------------
  /         \
 /           \
-------------------------------------------------------------------------
|super | GP| bg_blovk_bit_map| bg_Inode_bit_map|Inode Table| Data Bolcks |
-------------------------------------------------------------------------
            /                 \
    ==============BLOCKS BITMAP==============
    | BYTE0        : b7 b6 b5 b4 b3 b2 b1 b0 |               
    | BYTE1        : b7 b6 b5 b4 b3 b2 b1 b0 |                  
    | BYTE2        : b7 b6 b5 b4 b3 b2 b1 b0 |
    | BYTE..       : b7 b6 b5 b4 b3 b2 b1 b0 |
    | BYTE B_SIZE-1: b7 b6 b5 b4 b3 b2 b1 b0 |
    =========================================
*/

void block_bitmap(int num)
{
    // get the id of the first block of the block bitmap for the group represented
    __u32 index = myGroups[num].bg_block_bitmap;

    char* bg_bitmap = malloc(block_size * sizeof(char));
    
    // read the whole box into bg_bitmmap 
    if (pread(fd_image, bg_bitmap, block_size, index * (int) block_size) < 0)
    {
      fprintf(stderr, "Error reading from the bit map\n");
      exit(1);
    }
    unsigned int byte_index;
    unsigned int bit_index;
    int mask = 0x01;
   for (byte_index = 0; byte_index < block_size; byte_index++)
    {
        char byteRow = bg_bitmap[byte_index];
        for (bit_index = 0; bit_index < 8; bit_index++)
        {
            int bit = byteRow & mask;
            int block_num = super_block.s_blocks_per_group*num + byte_index * 8 + bit_index + 1;
            if (bit == 0)
            {
                printf("%s,%u\n","BFREE",block_num);
            }
            byteRow=byteRow>>1;
        }
    }
}


void inode_bitmap(int num)
{
    __u32 bitmap_num = myGroups[num].bg_inode_bitmap;
    char* bitmap = malloc(block_size * sizeof(char));

    if (pread(fd_image, bitmap, block_size, bitmap_num * (int) block_size) < 0)
    {
      fprintf(stderr, "Error Occured when reading from the bit map\n");
      exit(1);
    }
    unsigned int byte_index;
    unsigned int bit_index;
    int mask = 0x01;
    for (byte_index = 0; byte_index < block_size; byte_index++)
    {
        char byteRow = bitmap[byte_index];
        for (bit_index = 0; bit_index < 8; bit_index++)
        {
            int bit = byteRow & mask ;
            int inode_num = super_block.s_inodes_per_group*num + byte_index * 8 + bit_index  + 1;
            if (bit == 0)
            {
                printf("%s,%u\n","IFREE",inode_num);
            }
            byteRow = byteRow>>1;
        }
    }

}


/////////////////////////////////
// ERROR HANDLES
/////////////////////////////////
void error(char *msg)
{
    perror(msg);
    fprintf(stderr, "Exit code: %d, %s\n",errno,strerror(errno));
    exit(1);
}

int main(int argc, char* argv[])
{
  int i;

  if (argc != 2)
    error("Invalid Input, Please give an input ");
  
  // opend fd with only read option 
  if ((fd_image = open(argv[1], O_RDONLY)) < 0) {
      error("Error Occured when opeing the disk file discriptor");
  }
  // Same as read, except adding a fourth argument to specify the offset of the fd 
  if (pread(fd_image, &super_block, sizeof(struct ext2_super_block), SUPERBLOCK_OFFSET) < 0)
  {
    error("Error Occured when calling pread for superblock");
  } 
 /* calculate block size in bytes http://www.science.smith.edu/~nhowe/262/oldlabs/ext2.html#:~:text=A%20bitmap%20always%20refers%20to,the%20group%20it%20belongs%20to.*/
  block_size =  1024 << super_block.s_log_block_size;
  /* calculate number of block groups on the disk */
  num_groups = 1 + (super_block.s_blocks_count-1) / super_block.s_blocks_per_group;

  superblock_summary();
  myGroups = (struct ext2_group_desc*) malloc(num_groups * sizeof(struct ext2_group_desc));
  group_summary();

  for (i = 0; i < num_groups; i++)
  {
      block_bitmap(i);
      inode_bitmap(i);
      inode_table(i);
  }

  exit(0);
}


