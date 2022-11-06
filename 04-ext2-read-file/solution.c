#include <solution.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <ext2fs/ext2fs.h>

#define BOOT_BLOCK_SIZE 1024

int dump_file(int img, int inode_nr, int out) {
    (void) img;
    (void) inode_nr;
    (void) out;

    struct ext2_super_block super;
    unsigned int offset = BOOT_BLOCK_SIZE;
    int ret = pread(img, &super, sizeof(super), offset);
    offset += sizeof(super);
    if (ret < 0) {
        return -errno;
    }

    // Check if the file is an ext2 image
    if (super.s_magic != EXT2_SUPER_MAGIC) {
        return -errno;
    }

    // Get block size in bytes
    unsigned int BLOCK_SIZE = EXT2_MIN_BLOCK_SIZE << super.s_log_block_size;

    struct ext2_group_desc group_desc;
//    unsigned int group_count = 1 + (super.s_blocks_count - 1) / super.s_blocks_per_group;
//    unsigned int des_list_size = group_count * sizeof(struct ext2_group_desc);
    unsigned int group_desc_number = (inode_nr - 1) / super.s_inodes_per_group;
    offset = BOOT_BLOCK_SIZE + BLOCK_SIZE + group_desc_number * sizeof(struct ext2_group_desc);
    ret = pread(img, &group_desc, sizeof(group_desc), offset);
    if (ret < 0) {
        return -errno;
    }

    struct ext2_inode inode;
    unsigned int inode_index = (inode_nr - 1) % super.s_inodes_per_group;
    offset = group_desc.bg_inode_table * BLOCK_SIZE + (inode_index * super.s_inode_size);
    ret = pread(img, &inode, sizeof(inode), offset);
    if (ret < 0) {
        return -errno;
    }

    return 0;
}
