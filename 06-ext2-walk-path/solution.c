#include <solution.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <ext2fs/ext2fs.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <stdint.h>

int img_fd;
const char* file_name;
char file_type;
int inode_numb;
char name[FILENAME_MAX];

#define BOOT_BLOCK_SIZE 1024
unsigned BLOCK_SIZE = 1024;
unsigned file_data_left = 0;

int copy_direct_blocks(unsigned int i_block, int img, int out) {
    unsigned int offset = i_block * BLOCK_SIZE;
    char buffer[BLOCK_SIZE];
    unsigned int bytes_to_copy = file_data_left < BLOCK_SIZE ? file_data_left : BLOCK_SIZE;
    int ret = pread(img, &buffer, bytes_to_copy, offset);
    if (ret < 0) {
        return -errno;
    }

    ret = write(out, buffer, bytes_to_copy);
    if (ret < 0) {
        return -errno;
    }

    file_data_left -= bytes_to_copy;
    return 0;
}

int copy_indirect_blocks(unsigned int i_block, int img, int out) {
    unsigned int inode_buffer[BLOCK_SIZE];
    unsigned int offset = i_block * BLOCK_SIZE;
    int ret = pread(img, &inode_buffer, BLOCK_SIZE, offset);
    if (ret < 0) {
        return -errno;
    }
    unsigned int indirect_inode_size = BLOCK_SIZE / 4;
    for (unsigned int i = 0; i < indirect_inode_size; i++) {
        ret = copy_direct_blocks(inode_buffer[i], img, out);
        if (ret < 0) {
            return -errno;
        }
    }

    return 0;
}

int copy_double_indirect_blocks(unsigned int i_block, int img, int out) {
    unsigned int indirect_inode_buffer[BLOCK_SIZE];
    unsigned int offset = i_block * BLOCK_SIZE;
    int ret = pread(img, &indirect_inode_buffer, BLOCK_SIZE, offset);
    if (ret < 0) {
        return -errno;
    }

    unsigned int indirect_inode_size = BLOCK_SIZE / 4;
    for (unsigned int i = 0; i < indirect_inode_size; i++) {
        ret = copy_indirect_blocks(indirect_inode_buffer[i], img, out);
        if (ret < 0) {
            return -errno;
        }
    }

    return 0;
}

int dump_file_content(int img, int inode_nr, int out) {

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
            copy_direct_blocks(inode.i_block[i], img, out);
        }
        else if (i == EXT2_IND_BLOCK) {
            copy_indirect_blocks(inode.i_block[i], img, out);
        }
        else if (i == EXT2_DIND_BLOCK) {
            copy_double_indirect_blocks(inode.i_block[i], img, out);
        }
    }
    return 0;
}


int get_direct_blocks(unsigned i_block, int img) {
    unsigned offset = i_block * BLOCK_SIZE;
    unsigned bytes_to_read = file_data_left < BLOCK_SIZE ? file_data_left : BLOCK_SIZE;
    unsigned char direct_block_buffer[BLOCK_SIZE];

    int ret = pread(img, &direct_block_buffer, bytes_to_read, offset);
    file_data_left -= bytes_to_read;
    if (ret < 0) {
        return -errno;
    }

    unsigned size = 0;
    struct ext2_dir_entry_2 *entry = (struct ext2_dir_entry_2 *) direct_block_buffer;
    while (size < bytes_to_read) {
        entry = (void *) direct_block_buffer + size;
        size += entry->rec_len;

        unsigned inode = entry->inode;
        if (inode == 0) {
            break;
        }

        // Get file name
        char fileName[EXT2_NAME_LEN + 1];
        memcpy(fileName, entry->name, entry->name_len);
        fileName[entry->name_len] = '\0';

        // Get file type
        unsigned fileType = entry->file_type;
        char type;
        switch (fileType) {
            case EXT2_FT_DIR:
                type = 'd';
                break;
            case EXT2_FT_REG_FILE:
                type = 'f';
                break;
            default:
                return -errno;
        }

        // Check if found file is required one
        if (type == file_type && fileName == file_name) {
            inode_numb = inode;
        }
    }

    return 0;
}

int get_indirect_blocks(unsigned i_block, int img) {
    unsigned inode_buffer[BLOCK_SIZE];
    unsigned offset = i_block * BLOCK_SIZE;
    int ret = pread(img, &inode_buffer, BLOCK_SIZE, offset);
    if (ret < 0) {
        return -errno;
    }
    unsigned indirect_inode_size = BLOCK_SIZE / 4;
    for (unsigned i = 0; i < indirect_inode_size; i++) {
        ret = get_direct_blocks(inode_buffer[i], img);
        if (ret < 0) {
            return -errno;
        }
    }

    return 0;
}

int get_double_indirect_blocks(unsigned i_block, int img) {
    unsigned indirect_inode_buffer[BLOCK_SIZE];
    unsigned offset = i_block * BLOCK_SIZE;
    int ret = pread(img, &indirect_inode_buffer, BLOCK_SIZE, offset);
    if (ret < 0) {
        return -errno;
    }

    // Indirect block entirely consists of 4 byte entries
    unsigned indirect_inode_size = BLOCK_SIZE / 4;
    for (unsigned i = 0; i < indirect_inode_size; i++) {
        ret = get_indirect_blocks(indirect_inode_buffer[i], img);
        if (ret < 0) {
            return -errno;
        }
    }

    return 0;
}

int get_dir_inode(int img, int inode_nr) {
    // Get the ext2 superblock
    struct ext2_super_block super;
    unsigned offset = BOOT_BLOCK_SIZE;
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
    unsigned group_desc_number = (inode_nr - 1) / super.s_inodes_per_group;
    offset = BOOT_BLOCK_SIZE + BLOCK_SIZE + group_desc_number * sizeof(struct ext2_group_desc);
    ret = pread(img, &group_desc, sizeof(group_desc), offset);
    if (ret < 0) {
        return -errno;
    }

    // Get the required inode
    struct ext2_inode inode;
    unsigned inode_index = (inode_nr - 1) % super.s_inodes_per_group;
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
            get_direct_blocks(inode.i_block[i], img);
        } else if (i == EXT2_IND_BLOCK) {
            get_indirect_blocks(inode.i_block[i], img);
        } else if (i == EXT2_DIND_BLOCK) {
            get_double_indirect_blocks(inode.i_block[i], img);
        }
    }
    return 0;
}

char* get_next_dir_name(const char* ptr, unsigned* length) {
    char* endPtr = strpbrk(ptr + 1, "/");
    *length = endPtr - ptr - 1;
    char* dirName = ptr + 1;
    name = dirName;
    return endPtr;
}

int dump_file(int img, const char *path, int out) {
    img_fd = img;

    const char* charPtr = path;
    unsigned length;
    int inodeNumb = EXT2_ROOT_INO;

//    char* name = (char *)malloc(sizeof(char) * 256);

    while ((charPtr = get_next_dir_name(charPtr, &length))) {
        name[length] = '\0';

        file_name = name;
        file_type = 'd';
        inode_numb = -1;

        // search for required file's inode in current directory
        get_dir_inode(img_fd, inodeNumb);
        if (inode_numb == -1) {
            return -ENOENT;
        }

        inodeNumb = inode_numb;
    }


    file_name = charPtr + 1;
    file_type = 'f';
    inode_numb = -1;

    get_dir_inode(img_fd, inodeNumb);
    if (inode_numb == -1) {
        return -ENOENT;
    }

    inodeNumb = inode_numb;
    dump_file_content(img, inodeNumb, out);

    return 0;
}
