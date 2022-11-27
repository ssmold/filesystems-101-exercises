#include <solution.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <fuse.h>
#include <errno.h>
#include <sys/stat.h>
#include <fuse.h>
#include <ext2fs/ext2fs.h>


#define BOOT_BLOCK_SIZE 1024
static int fs_img;
static struct ext2_super_block super;
static unsigned BLOCK_SIZE = 1024;


// for dump_content
const char* file_name;
char file_type;
int inode_numb;
char name[FILENAME_MAX];

// for readdir
fuse_fill_dir_t filler;
void* buffer;

// for read
unsigned file_data_left = 0;
char* file_buffer;
int buffer_ptr = 0;


void set_attributes() {
}

int get_ext2_info() {
    // Get the ext2 superblock
    unsigned offset = BOOT_BLOCK_SIZE;
    int ret = pread(fs_img, &super, sizeof(super), offset);
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
    return 0;
}

int copy_direct_blocks(unsigned int i_block, int img) {
    unsigned int offset = i_block * BLOCK_SIZE;
    unsigned int bytes_to_copy = file_data_left < BLOCK_SIZE ? file_data_left : BLOCK_SIZE;
    int ret = pread(img, file_buffer + buffer_ptr, bytes_to_copy, offset);
    if (ret < 0) {
        return -errno;
    }

    buffer_ptr += bytes_to_copy;
    file_data_left -= bytes_to_copy;
    return 0;
}

int copy_indirect_blocks(unsigned int i_block, int img) {
    unsigned int inode_buffer[BLOCK_SIZE];
    unsigned int offset = i_block * BLOCK_SIZE;
    int ret = pread(img, &inode_buffer, BLOCK_SIZE, offset);
    if (ret < 0) {
        return -errno;
    }
    unsigned int indirect_inode_size = BLOCK_SIZE / 4;
    for (unsigned int i = 0; i < indirect_inode_size; i++) {
        ret = copy_direct_blocks(inode_buffer[i], img);
        if (ret < 0) {
            return -errno;
        }
    }

    return 0;
}

int copy_double_indirect_blocks(unsigned int i_block, int img) {
    unsigned int indirect_inode_buffer[BLOCK_SIZE];
    unsigned int offset = i_block * BLOCK_SIZE;
    int ret = pread(img, &indirect_inode_buffer, BLOCK_SIZE, offset);
    if (ret < 0) {
        return -errno;
    }

    unsigned int indirect_inode_size = BLOCK_SIZE / 4;
    for (unsigned int i = 0; i < indirect_inode_size; i++) {
        ret = copy_indirect_blocks(indirect_inode_buffer[i], img);
        if (ret < 0) {
            return -errno;
        }
    }

    return 0;
}

int dump_file_content(int img, int inode_nr, int* len) {
    int offset;
    int ret;

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
    *len = inode.i_size;
    file_buffer = (char*)malloc(file_data_left);
    buffer_ptr = 0;

    // Copy data
    for(int i = 0; i < EXT2_N_BLOCKS; i++) {
        if (i < EXT2_NDIR_BLOCKS) {
            copy_direct_blocks(inode.i_block[i], img);
        }
        else if (i == EXT2_IND_BLOCK) {
            copy_indirect_blocks(inode.i_block[i], img);
        }
        else if (i == EXT2_DIND_BLOCK) {
            copy_double_indirect_blocks(inode.i_block[i], img);
        }
    }
    return 0;
}


int read_direct_blocks(unsigned i_block, int img) {
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

//        struct stat *st;
//        st->st_ino = inode;
//        st->st_blocks = inode->i_blocks;
//        st->st_size = inode->i_size;
//        st->st_mode = inode->i_mode;
//        st->st_nlink = inode->i_links_count;
//        st->st_uid = inode->i_uid;
//        st->st_gid = inode->i_gid;

        // Get file name
        char file_name[EXT2_NAME_LEN + 1];
        memcpy(file_name, entry->name, entry->name_len);
        file_name[entry->name_len] = '\0';

        // Get file type
        unsigned file_type = entry->file_type;
//        char type;
        switch (file_type) {
            case EXT2_FT_DIR:
                //type = 'd';
                break;
            case EXT2_FT_REG_FILE:
                //type = 'f';
                break;
            default:
                return -errno;
        }
        filler(buffer, file_name, 0, 0, FUSE_FILL_DIR_PLUS);
    }

    return 0;
}

int read_indirect_blocks(unsigned i_block, int img) {
    unsigned inode_buffer[BLOCK_SIZE];
    unsigned offset = i_block * BLOCK_SIZE;
    int ret = pread(img, &inode_buffer, BLOCK_SIZE, offset);
    if (ret < 0) {
        return -errno;
    }
    unsigned indirect_inode_size = BLOCK_SIZE / 4;
    for (unsigned i = 0; i < indirect_inode_size; i++) {
        ret = read_direct_blocks(inode_buffer[i], img);
        if (ret < 0) {
            return -errno;
        }
    }

    return 0;
}

int read_double_indirect_blocks(unsigned i_block, int img) {
    unsigned indirect_inode_buffer[BLOCK_SIZE];
    unsigned offset = i_block * BLOCK_SIZE;
    int ret = pread(img, &indirect_inode_buffer, BLOCK_SIZE, offset);
    if (ret < 0) {
        return -errno;
    }

    // Indirect block entirely consists of 4 byte entries
    unsigned indirect_inode_size = BLOCK_SIZE / 4;
    for (unsigned i = 0; i < indirect_inode_size; i++) {
        ret = read_indirect_blocks(indirect_inode_buffer[i], img);
        if (ret < 0) {
            return -errno;
        }
    }

    return 0;
}

int dump_content(int img, int inode_nr) {
    int offset;
    int ret;

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
            read_direct_blocks(inode.i_block[i], img);
        } else if (i == EXT2_IND_BLOCK) {
            read_indirect_blocks(inode.i_block[i], img);
        } else if (i == EXT2_DIND_BLOCK) {
            read_double_indirect_blocks(inode.i_block[i], img);
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
        char type ;
        switch (fileType) {
            case EXT2_FT_DIR:
                type = 'd';
                break;
            case EXT2_FT_REG_FILE:
                type = 'f';
                break;
            default:
                return -EINVAL;
        }

        // Check if found file is required one
        if ((strcmp(fileName, file_name) == 0)) {
            if (type == file_type) {
                inode_numb = inode;
            } else {
                return -ENOTDIR;
            }
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
            return ret;
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
            return ret;
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

    // Find inode of required file/dir
    for (int i = 0; i < EXT2_N_BLOCKS; i++) {
        if (i < EXT2_NDIR_BLOCKS) {
            ret = get_direct_blocks(inode.i_block[i], img);
            if (ret < 0) {
                return ret;
            }
        } else if (i == EXT2_IND_BLOCK) {
            ret = get_indirect_blocks(inode.i_block[i], img);
            if (ret < 0) {
                return ret;
            }
        } else if (i == EXT2_DIND_BLOCK) {
            ret = get_double_indirect_blocks(inode.i_block[i], img);
            if (ret < 0) {
                return ret;
            }
        }
    }
    return 0;
}

char* get_next_dir_name(const char* ptr) {
    char* endPtr = strpbrk(ptr + 1, "/");
    if (endPtr == NULL) {
        return NULL;
    }

    unsigned length = endPtr - ptr - 1;
    const char* dirName;
    dirName = ptr + 1;
    strncpy(name, dirName, length);
    name[length] = '\0';
    return endPtr;
}

int dump_file(int img, const char *path, int *len) {

    const char* charPtr = path;
    int inodeNumb = EXT2_ROOT_INO;

    while ((charPtr = get_next_dir_name(charPtr))) {

        file_name = name;
        file_type = 'd';
        inode_numb = -1;

        // search for required file's inode in current directory
        int ret = get_dir_inode(img, inodeNumb);
        if (ret < 0) {
            return ret;
        }

        if (inode_numb == -1) {
            return -ENOENT;
        }
        inodeNumb = inode_numb;
    }

    strcpy(name, path);
    file_name = basename(name);
    file_type = 'f';
    inode_numb = -1;

    get_dir_inode(img, inodeNumb);
    if (inode_numb == -1) {
        return -ENOENT;
    }

    inodeNumb = inode_numb;
    return dump_file_content(img, inodeNumb, len);
}

static int read_impl(const char *path, char *buf, size_t size, off_t offset,
                     struct fuse_file_info *fi __attribute__((unused))) {

    int len = 0;
    dump_file(fs_img, path, &len);


    size = len - offset;
    memcpy(buf, file_buffer + offset, size);
    free(file_buffer);

    return size;
}

static int readdir_impl(const char *path, void *buf, fuse_fill_dir_t fil, off_t off,
                        struct fuse_file_info *ffi,  enum fuse_readdir_flags frf)
{
    (void) ffi;
    (void) frf;
    (void) off;

    buffer = buf;
    filler = fil;

    const char* charPtr = path;
    int inodeNumb = EXT2_ROOT_INO;

    while ((charPtr = get_next_dir_name(charPtr))) {

        file_name = name;
        file_type = 'd';
        inode_numb = -1;

        // search for required file's inode in current directory
        int ret = get_dir_inode(fs_img, inodeNumb);
        if (ret < 0) {
            return ret;
        }

        if (inode_numb == -1) {
            return -ENOENT;
        }
        inodeNumb = inode_numb;
    }

//    strcpy(name, path);
//    file_name = basename(name);
//    file_type = 'd';
//    inode_numb = -1;
//
//    get_dir_inode(fs_img, inodeNumb);
//    if (inode_numb == -1) {
//        return -ENOENT;
//    }
//
//    inodeNumb = inode_numb;
    return dump_content(fs_img, inodeNumb);
}

static int set_attr(const char* path, struct stat *st) {
    int offset;
    int ret;
    const char* charPtr = path;
    int inodeNumb = EXT2_ROOT_INO;

    while ((charPtr = get_next_dir_name(charPtr))) {

        file_name = name;
        file_type = 'd';
        inode_numb = -1;

        // search for required file's inode in current directory
        int ret = get_dir_inode(fs_img, inodeNumb);
        if (ret < 0) {
            return ret;
        }

        if (inode_numb == -1) {
            return -ENOENT;
        }
        inodeNumb = inode_numb;
    }

    strcpy(name, path);
    file_name = basename(name);
    file_type = 'd';
    inode_numb = -1;

    get_dir_inode(fs_img, inodeNumb);
    if (inode_numb == -1) {
        return -ENOENT;
    }

    int inode_nr = inode_numb;

    if (inode_nr < 0)
        return -ENOENT;

    struct ext2_group_desc group_desc;
    unsigned group_desc_number = (inode_nr - 1) / super.s_inodes_per_group;
    offset = BOOT_BLOCK_SIZE + BLOCK_SIZE + group_desc_number * sizeof(struct ext2_group_desc);
    ret = pread(fs_img, &group_desc, sizeof(group_desc), offset);
    if (ret < 0) {
        return -errno;
    }

    // Get the required inode
    struct ext2_inode inode;
    unsigned inode_index = (inode_nr - 1) % super.s_inodes_per_group;
    offset = group_desc.bg_inode_table * BLOCK_SIZE + (inode_index * super.s_inode_size);
    ret = pread(fs_img, &inode, sizeof(inode), offset);
    if (ret < 0) {
        return -errno;
    }


    st->st_ino = inode_nr;
    st->st_blocks = inode.i_blocks;
    st->st_size = inode.i_size;
    st->st_mode = inode.i_mode;
    st->st_nlink = inode.i_links_count;
    st->st_uid = inode.i_uid;
    st->st_gid = inode.i_gid;

    return 0;
}

static int getattr_impl( const char *path, struct stat *st, struct fuse_file_info *fi __attribute__((unused))) {
    int ret;
    ret = set_attr(path, st);
    if (ret < 0) {
        return ret;
    }
    return 0;
}


static int write_impl(const char *path __attribute__((unused)),
                      const char *buffer __attribute__((unused)),
                      size_t size __attribute__((unused)),
                      off_t offset __attribute__((unused)),
                      struct fuse_file_info *fi __attribute__((unused))) {

    return -EROFS;
}

static int open_impl(const char *path __attribute__((unused)),
                     struct fuse_file_info *fi) {

    if ((fi->flags & O_ACCMODE) != O_RDONLY)
        return -EROFS;
    return 0;
}

static int create_impl(const char *path __attribute__((unused)),
                       mode_t mode __attribute__((unused)),
                       struct fuse_file_info *fi __attribute__((unused))) {
    return -EROFS;
}

static const struct fuse_operations ext2_ops = {
        .getattr = getattr_impl,
        .readdir = readdir_impl,
        .read = read_impl,

         // Write operations
        .write = write_impl,
        .open = open_impl,
        .create = create_impl,
};

int ext2fuse(int img, const char *mntp)
{
    int ret;
	fs_img = img;
    ret = get_ext2_info();
    if (ret < 0) {
        return ret;
    }

	char *argv[] = {"exercise", "-f", (char *)mntp, NULL};
	return fuse_main(3, argv, &ext2_ops, NULL);
}
