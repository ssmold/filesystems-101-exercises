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

static const int BLOCK_SIZE = 4096;

static int getattr_impl(const char *path, struct stat *st,
		struct fuse_file_info *fi __attribute__((unused))) {
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
		off_t offset __attribute__((unused)),
		struct fuse_file_info *fi __attribute__((unused)),
		enum fuse_readdir_flags flags __attribute__((unused))) {

	filler(buffer, ".", NULL, 0, FUSE_FILL_DIR_PLUS);
	filler(buffer, "..", NULL, 0, FUSE_FILL_DIR_PLUS);

	if (strcmp(path, "/") == 0) {
		filler(buffer, "hello", NULL, 0, FUSE_FILL_DIR_PLUS);
	}

	return 0;
}

static int read_impl(const char *path, char *buffer, size_t size, off_t offset,
		struct fuse_file_info *fi __attribute__((unused))) {
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

static int write_impl(const char *path __attribute__((unused)),
		const char *buffer __attribute__((unused)),
		size_t size __attribute__((unused)),
		off_t offset __attribute__((unused)),
		struct fuse_file_info *fi __attribute__((unused))) {
	return -EROFS;
}

static int mknod_impl(const char *name __attribute__((unused)),
		mode_t mode __attribute__((unused)), dev_t dev __attribute__((unused))) {
	return -EROFS;
}

static int mkdir_impl(const char *path __attribute__((unused)),
		mode_t mode __attribute__((unused))) {
	return -EROFS;
}

//static int ftruncate_impl(const char *path __attribute__((unused)),
//		off_t offset __attribute__((unused)),
//		struct fuse_file_info *fi __attribute__((unused))
//		) {
//	return -EROFS;
//}

static int create_impl(const char *path __attribute__((unused)),
		mode_t mode __attribute__((unused)),
		struct fuse_file_info *fi __attribute__((unused))) {
	return -EROFS;
}

static int removexattr_impl(const char *path __attribute__((unused)),
		const char *buffer __attribute__((unused))) {
	return -EROFS;
}

static int setxattr_impl(const char *path __attribute__((unused)),
		const char *buf __attribute__((unused)),
		const char *w_buf __attribute__((unused)),
		size_t size __attribute__((unused)), int n __attribute__((unused))) {
	return -EROFS;
}

static int truncate_impl(const char *path __attribute__((unused)),
		off_t offset __attribute__((unused)),
		struct fuse_file_info *fi __attribute__((unused))) {
	return -EROFS;
}

static int unlink_impl(const char *path __attribute__((unused))) {
	return -EROFS;
}

static int rmdir_impl(const char *path __attribute__((unused))) {
	return -EROFS;
}

static int symlink_impl(const char *path_from __attribute__((unused)),
		const char *path_to __attribute__((unused))) {
	return -EROFS;
}

static int rename_impl(const char *path_from __attribute__((unused)),
		const char *path_to __attribute__((unused)),
		unsigned int n __attribute__((unused))) {
	return -EROFS;
}

static int link_impl(const char *path_from __attribute__((unused)),
		const char *path_to __attribute__((unused))) {
	return -EROFS;
}

static int chmod_impl(const char *path __attribute__((unused)),
		mode_t mode __attribute__((unused))) {
	return -EROFS;
}

static int* chown_impl(const char *path __attribute__((unused)),
		uid_t uid __attribute__((unused)), gid_t gid __attribute__((unused))) {
	return -EROFS;
}

static const struct fuse_operations hellofs_ops = { .getattr = getattr_impl,
		.readdir = readdir_impl, .read = read_impl, .write = write_impl,
		.mknod = mknod_impl, .mkdir = mkdir_impl, .create = create_impl,
		.removexattr = removexattr_impl, .setxattr = setxattr_impl, .truncate =
				truncate_impl, .rmdir = rmdir_impl, .symlink = symlink_impl,
		.rename = rename_impl, .link = link_impl, .unlink = unlink_impl,
		.chmod = chmod_impl, .chown = chown_impl };

int helloworld(const char *mntp) {
	char *argv[] = { "exercise", "-f", (char*) mntp, NULL };
	return fuse_main(3, argv, &hellofs_ops, NULL);
}
