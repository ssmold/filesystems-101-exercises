#include <solution.h>
#include <fcntl.h>
#include <dirent.h>
#include <ctype.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

void lsof(void)
{
	DIR* dir = opendir("/proc");

	if (dir == NULL) {
		report_error("/proc", errno);
		return;
	}

	struct dirent* ent;
	while ((ent = readdir(dir)) != NULL) {
		int isProcess = 1;
		for (int i = 0; ent->d_name[i]; i++) {
			if (!isdigit(ent->d_name[i])) {
				isProcess = 0;
				break;
			}
		}

		if (!isProcess) {
			continue;
		}

		char process_fd[PATH_MAX];

		sprintf(process_fd, "/proc/%s/fd", ent->d_name);
		DIR* fd_dir = opendir(process_fd);

		if (fd_dir == NULL) {
			report_error(process_fd, errno);
			closedir(dir);
			return;
		}

		struct dirent* fd_ent;
		while((fd_ent = readdir(fd_dir)) != NULL) {
			char fd_path[PATH_MAX];
			char real_fd_path[PATH_MAX];

			sprintf(fd_path, "/proc/%s/fd/%s", ent->d_name, fd_ent->d_name);
			realpath(fd_path, real_fd_path);

			report_file(real_fd_path);
		}
		closedir(fd_dir);
	}
	closedir(dir);
}
