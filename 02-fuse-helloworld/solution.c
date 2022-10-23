#include <solution.h>
#include <fuse.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <fuse.h>
#include <errno.h>
#include <sys/stat.h>

static const int BLOCK_SIZE = 4096;
static const int TEXT_SIZE = 64;

static int getattr_impl(const char *path, struct stat *st,
		struct fuse_file_info *fi __attribute__((unused))) {

	st->st_uid = getuid();
	st->st_gid = getgid();
	st->st_atime = time( NULL);
	st->st_mtime = time( NULL);

	if (strcmp(path, "/") == 0) {
		st->st_mode = S_IFDIR | 0555;
		st->st_nlink = 2;
	} else if (strcmp(path, ".") == 0 || strcmp(path, "..") == 0
			|| strcmp(path, "/hello") == 0) {
		st->st_mode = S_IFREG | 0444;
		st->st_nlink = 1;
		st->st_size = BLOCK_SIZE;
	} else {
		return -ENOENT;
	}

	return 0;
}

static int readdir_impl(const char *path, void *buffer, fuse_fill_dir_t filler,
		off_t offset __attribute__((unused)),
		struct fuse_file_info *fi __attribute__((unused)),
		enum fuse_readdir_flags flags __attribute__((unused))) {

	if (strcmp(path, "/") != 0) {
		return -ENOENT;
	}

	filler(buffer, ".", NULL, 0, FUSE_FILL_DIR_PLUS);
	filler(buffer, "..", NULL, 0, FUSE_FILL_DIR_PLUS);
	filler(buffer, "hello", NULL, 0, FUSE_FILL_DIR_PLUS);

	return 0;
}

static int read_impl(const char *path, char *buffer, size_t size, off_t offset,
		struct fuse_file_info *fi __attribute__((unused))) {

	if (strcmp(path, "/hello") != 0) {
		return -ENOENT;
	}

	pid_t current_pid = fuse_get_context()->pid;
	char text[TEXT_SIZE];

	sprintf(text, "hello, %d\n", current_pid);

	memcpy(buffer, text + offset, size);
	if (offset < strlen(text)) {
		return strlen(text) - offset;
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

static const struct fuse_operations hellofs_ops = { .getattr = getattr_impl,
		.readdir = readdir_impl, .read = read_impl, .write = write_impl, .open =
				open_impl, .create = create_impl, };

int helloworld(const char *mntp) {
	char *argv[] = { "exercise", "-f", (char*) mntp, NULL };
	return fuse_main(3, argv, &hellofs_ops, NULL);
}
