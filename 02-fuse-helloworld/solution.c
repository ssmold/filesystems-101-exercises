#include <solution.h>
#include <fuse.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <fuse.h>

static const int BLOCK_SIZE = 4096;

static int getattr_impl(const char *path, struct stat *st) {
	st->st_uid = getuid();
	st->st_gid = getgid();
	st->st_atime = time( NULL);
	st->st_mtime = time( NULL);

	if (strcmp(path, "/") == 0) {
		st->st_mode = S_IFDIR | 0755;
		st->st_nlink = 2;
	} else {
		st->st_mode = S_IFREG | 0644;
		st->st_nlink = 1;
		st->st_size = BLOCK_SIZE;
	}

	return 0;
}

static int readdir_impl(const char *path, void *buffer, fuse_fill_dir_t filler,
		off_t offset, struct fuse_file_info *fi) {

	filler(buffer, ".", NULL, 0, FUSE_READDIR_PLUS);
	filler(buffer, "..", NULL, 0, FUSE_READDIR_PLUS);

	if (strcmp(path, "/") == 0) {
		filler(buffer, "hello", NULL, 0);
	}

	return 0;
}

static int read_impl(const char *path, char *buffer, size_t size, off_t offset,
		struct fuse_file_info *fi) {
	pid_t current_pid = fuse_get_context()->pid;
	char text[50];

	if (strcmp(path, "/hello") == 0) {
		sprintf(text, "hello, %d\n", current_pid);
	} else {
		return -1;
	}

	memcpy(buffer, text + offset, size);
	return strlen(text) - offset;
}

static int write_impl(const char*, char*, size_t, off_t, struct fuse_file_info*) {
	return -EROFS;
}

static const struct fuse_operations hellofs_ops = { .getattr = getattr_impl,
		.readdir = readdir_impl, .read = read_impl, .write = write_impl };

int helloworld(const char *mntp) {
	char *argv[] = { "exercise", "-f", (char*) mntp, NULL };
	return fuse_main(3, argv, &hellofs_ops, NULL);
}
