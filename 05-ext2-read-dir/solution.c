#include <solution.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <ext2fs/ext2fs.h>
#include <stdint.h>
#include <string.h>


#define BOOT_BLOCK_SIZE 1024
unsigned int BLOCK_SIZE = 1024;
unsigned int file_data_left;

int read_direct_blocks(unsigned int i_block, int img) {
    unsigned int offset = i_block * BLOCK_SIZE;
    unsigned int bytes_to_read = file_data_left < BLOCK_SIZE ? file_data_left : BLOCK_SIZE;
    unsigned char direct_block_buffer[BLOCK_SIZE];

    int ret = pread(img, &direct_block_buffer, bytes_to_read, offset);
    file_data_left -= bytes_to_read;
    if (ret < 0) {
        return -errno;
    }

    unsigned int size = 0;
    struct ext2_dir_entry_2* entry = (struct ext2_dir_entry_2 *) direct_block_buffer;
    while (size < bytes_to_read) {
        unsigned int inode = entry->inode;
        if (inode == 0) {
            break;
        }

        // Get file name
        char file_name[EXT2_NAME_LEN + 1];
        memcpy(file_name, entry->name, entry->name_len);
        file_name[entry->name_len] = '\0';

        // Get file type
        unsigned int file_type = entry->file_type;
        char type;
        switch (file_type) {
            case EXT2_FT_DIR:
                type = 'd';
                break;
            case EXT2_FT_REG_FILE:
                type = 'f';
                break;
            default:
                return -errno;
        }

        report_file(inode, type, file_name);

        // Move to the next entry
        entry = (void *) entry + entry->rec_len;
        size += entry->rec_len;
    }

    return 0;
}

int read_indirect_blocks(unsigned int i_block, int img) {
    unsigned int inode_buffer[BLOCK_SIZE];
    unsigned int offset = i_block * BLOCK_SIZE;
    int ret = pread(img, &inode_buffer, BLOCK_SIZE, offset);
    if (ret < 0) {
        return -errno;
    }
    unsigned int indirect_inode_size = BLOCK_SIZE / 4;
    for (unsigned int i = 0; i < indirect_inode_size; i++) {
        ret = read_direct_blocks(inode_buffer[i], img);
        if (ret < 0) {
            return -errno;
        }
    }

    return 0;
}

int read_double_indirect_blocks(unsigned int i_block, int img) {
    unsigned int indirect_inode_buffer[BLOCK_SIZE];
    unsigned int offset = i_block * BLOCK_SIZE;
    int ret = pread(img, &indirect_inode_buffer, BLOCK_SIZE, offset);
    if (ret < 0) {
        return -errno;
    }

    unsigned int indirect_inode_size = BLOCK_SIZE / 4;
    for (unsigned int i = 0; i < indirect_inode_size; i++) {
        ret = read_indirect_blocks(indirect_inode_buffer[i], img);
        if (ret < 0) {
            return -errno;
        }
    }

    return 0;
}

int dump_dir(int img, int inode_nr) {
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
    for (int i = 0; i < EXT2_N_BLOCKS; i++) {
        if (i < EXT2_NDIR_BLOCKS) {
            read_direct_blocks(inode.i_block[i], img);
        } else if (i == EXT2_IND_BLOCK) {
            read_indirect_blocks(inode.i_block[i], img);
        } else if (i == EXT2_DIND_BLOCK) {
            read_double_indirect_blocks(inode.i_block[i], img);
        }
    }
    return 0;
}
