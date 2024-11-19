#include "ext2_structures.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// constants
#define BLOCKSIZE 4096
uint16_t INODESIZE;
uint32_t FIRST_INODE_BLOCK;
uint32_t INODES_PER_GROUP;

int fd;

void read_block_into_buffer(unsigned char *buffer, uint32_t blocknum) {
  off_t offset = blocknum * BLOCKSIZE;
  lseek(fd, offset, SEEK_SET);
  if (read(fd, buffer, BLOCKSIZE) != BLOCKSIZE) {
    fprintf(stderr, "ERROR: block read is unsuccessful\n");
    exit(1);
  }
}

struct inode get_inode(uint32_t inodenum) {
  uint32_t inode_index = (inodenum - 1) % INODES_PER_GROUP;
  uint32_t inode_blocknum =
      FIRST_INODE_BLOCK + (inode_index / (BLOCKSIZE / INODESIZE));
  unsigned char buffer[BLOCKSIZE];
  read_block_into_buffer(buffer, inode_blocknum);
  uint32_t inode_offset_in_block =
      (inode_index % (BLOCKSIZE / INODESIZE)) * INODESIZE;
  return *((struct inode *)(buffer + inode_offset_in_block));
}

void convert_epoch_to_datetime(char *buffer, time_t epoch) {
  struct tm ts;
  ts = *localtime(&epoch);
  strftime(buffer, 80, "%a %Y-%m-%d %H:%M:%S %Z", &ts);
}

int main(int argc, char *argv[]) {
  const char *disk_name = argv[1];
  fd = open(disk_name, O_RDONLY);
  if (fd < 0) {
    fprintf(stderr, "ERROR: cannot open disk file\n");
    exit(1);
  }
  unsigned char buffer[BLOCKSIZE];
  uint32_t blocknum;
  uint32_t inodenum;

  printf("----------SUPERBLOCK INFORMATION----------\n\n");
  // reading first block into the buffer
  blocknum = 0;
  read_block_into_buffer(buffer, blocknum);

  // superblock information begins with 1024. byte of the first block
  struct superblock sb = *((struct superblock *)(buffer + 1024));
  printf("1) s_inodes_count: %d\n", sb.s_inodes_count);
  printf("2) s_blocks_count: %d\n", sb.s_blocks_count);
  printf("3) s_r_blocks_count: %d\n", sb.s_r_blocks_count);
  printf("4) s_free_blocks_count: %d\n", sb.s_free_blocks_count);
  printf("5) s_free_inodes_count: %d\n", sb.s_free_inodes_count);
  printf("6) s_first_data_block: %d\n", sb.s_first_data_block);
  printf("7) s_log_block_size: %d\n", sb.s_log_block_size);
  printf("8) s_blocks_per_group: %d\n", sb.s_blocks_per_group);
  printf("9) s_inodes_per_group: %d\n", sb.s_inodes_per_group);
  printf("10) s_inode_size: %d\n\n", sb.s_inode_size);

  // below code was used to find out the place of inode table
  blocknum = 1;
  read_block_into_buffer(buffer, blocknum);

  // block group descriptor table which is located at block 1 contains
  // information about the block number of inode table
  struct block_group_descriptor_table bgdt =
      *((struct block_group_descriptor_table *)buffer);

  // setting global properties needed while accessing inodes
  INODESIZE = sb.s_inode_size;
  INODES_PER_GROUP = sb.s_inodes_per_group;
  FIRST_INODE_BLOCK = bgdt.bg_inode_table;

  /* accessing root directory */
  printf("--------ROOT DIRECTORY INFORMATION--------\n\n");

  // 2. entry of the inode table contains the inode pointing to the data of
  // the root directory
  inodenum = 2;
  struct inode inode_root = get_inode(inodenum);

  // looping through directory entries
  // root directory entries may not fit into a single block
  for (int i = 0; i < inode_root.i_blocks / (2 << sb.s_log_block_size); i++) {
    uint32_t blocknum_root_data = inode_root.i_block[i];
    read_block_into_buffer(buffer, blocknum_root_data);
    int sum_rec_len = 0;
    // reading the first entry
    unsigned char *ptr_to_entry = buffer;

    while (sum_rec_len < BLOCKSIZE) {
      struct directory_entry_without_name entry =
          *((struct directory_entry_without_name *)ptr_to_entry);
      char file_name[256] = {0};
      memcpy(file_name, ptr_to_entry + 8, entry.name_len);
      printf("%s\n", file_name);

      // pointing to the next entry
      ptr_to_entry += entry.rec_len;
      sum_rec_len += entry.rec_len;
    }
  }

  printf("\n");
  printf("------------INODE INFORMATION-------------\n\n");

  for (int i = 0; i < inode_root.i_blocks / (2 << sb.s_log_block_size); i++) {
    uint32_t blocknum_root_data = inode_root.i_block[i];
    read_block_into_buffer(buffer, blocknum_root_data);
    int sum_rec_len = 0;
    // reading the first entry
    unsigned char *ptr_to_entry = buffer;

    while (sum_rec_len < BLOCKSIZE) {
      struct directory_entry_without_name entry =
          *((struct directory_entry_without_name *)ptr_to_entry);

      struct inode inode_of_regular_file = get_inode(entry.inode);

      char file_name[256] = {0};
      memcpy(file_name, ptr_to_entry + 8, entry.name_len);
      printf("Inode information of '%s'\n", file_name);
      printf("Inode no: %d\n", entry.inode);

      // below buffer is used for converting epoch time to human readable
      // format
      char timebuf[80];

      printf("1) i_uid: %d\n", inode_of_regular_file.i_uid);
      printf("2) i_size: %d\n", inode_of_regular_file.i_size);
      convert_epoch_to_datetime(timebuf, inode_of_regular_file.i_atime);
      printf("3) i_atime: %s\n", timebuf);
      convert_epoch_to_datetime(timebuf, inode_of_regular_file.i_ctime);
      printf("4) i_ctime: %s\n", timebuf);
      convert_epoch_to_datetime(timebuf, inode_of_regular_file.i_mtime);
      printf("5) i_mtime: %s\n", timebuf);
      convert_epoch_to_datetime(timebuf, inode_of_regular_file.i_dtime);
      printf("6) i_dtime: %s // epoch = 0 means inode is never deleted\n",
             timebuf);
      printf("7) i_gid: %d\n", inode_of_regular_file.i_gid);
      printf("8) i_links_count: %d\n", inode_of_regular_file.i_links_count);
      printf("9) i_blocks: %d\n", inode_of_regular_file.i_blocks);
      printf("10) i_flags: %d\n\n", inode_of_regular_file.i_flags);

      // pointing to the next entry
      ptr_to_entry += entry.rec_len;
      sum_rec_len += entry.rec_len;
    }
  }
  close(fd);

  return 0;
}
