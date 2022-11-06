#include <solution.h>
//#include <linux/ext2_fs.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

int dump_file(int img, int inode_nr, int out)
{
	(void) img;
	(void) inode_nr;
	(void) out;

    struct ext2_super_block super;
    pread(img, &super, sizeof(super), 1024)

    // Check if the file is an ext2 image
    if (super.s_magic != EXT2_SUPER_MAGIC) {
        return -errno;
    }

    // Get block size in bytes
    unsigned int block_size = 1024 << super.s_log_block;






	return 0;
}
