#include <solution.h>

int dump_dir(int img, int inode_nr)
{
    // Get the ext2 superblock
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
    BLOCK_SIZE = EXT2_MIN_BLOCK_SIZE << super.s_log_block_size;

    // Get the group descriptor by inode number
    struct ext2_group_desc group_desc;
    unsigned int group_desc_number = (inode_nr - 1) / super.s_inodes_per_group;
    offset = BOOT_BLOCK_SIZE + BLOCK_SIZE + group_desc_number * sizeof(struct ext2_group_desc);
    ret = pread(img, &group_desc, sizeof(group_desc), offset);
    if (ret < 0) {
        return -errno;
    }

    // Get the required inode
    struct ext2_inode inode;
    unsigned int inode_index = (inode_nr - 1) % super.s_inodes_per_group;
    offset = group_desc.bg_inode_table * BLOCK_SIZE + (inode_index * super.s_inode_size);
    ret = pread(img, &inode, sizeof(inode), offset);
    if (ret < 0) {
        return -errno;
    }

    // Get the file size
    file_data_left = inode.i_size;

    // Copy data
    for(int i = 0; i < EXT2_N_BLOCKS; i++) {
        if (i < EXT2_NDIR_BLOCKS) {
            //copy_direct_blocks(inode.i_block[i], img, out);
        }
        else if (i == EXT2_IND_BLOCK) {
            //copy_indirect_blocks(inode.i_block[i], img, out);
        }
        else if (i == EXT2_DIND_BLOCK) {
            //copy_double_indirect_blocks(inode.i_block[i], img, out);
        }
    }
    return 0;
}
