#include "solution.h"
#include <fcntl.h>
#include <dirent.h>
#include <ctype.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#define BUFF_SIZE 65536
#define MAX_ARGS 4096
#define MAX_LINE_SIZE 32768
#define MAX_VARS 4096

char cmd_path[PATH_MAX];
char cmd_buffer[BUFF_SIZE];
char env_path[PATH_MAX];
char env_buffer[BUFF_SIZE];
int argv_size = MAX_ARGS;
int vars_size = MAX_VARS;

void clean_up(char **argv, char **envp, int argv_size, int vars_size) {
	for (int i = 0; i < argv_size; i++) {
		free(argv[i]);
	}

	free(argv);

	for (int i = 0; i < vars_size; i++) {
		free(envp[i]);
	}
	free(envp);
}

void parse_argv(char **argv, char **vars, DIR *dir, struct dirent *ent) {
	sprintf(cmd_path, "/proc/%s/cmdline", ent->d_name);
	int cmd_fd = open(cmd_path, O_RDONLY);
	if (cmd_fd == -1) {
		report_error(cmd_path, errno);
		clean_up(argv, vars, argv_size, vars_size);
		closedir(dir);
		return;
	}

	int n_bytes_read = read(cmd_fd, cmd_buffer, BUFF_SIZE);
	if (n_bytes_read == -1) {
		report_error(cmd_path, errno);
		for (int i = 0; i < MAX_ARGS; i++) {
			free(argv[i]);
		}
		free(argv);
		close(cmd_fd);
		closedir(dir);
		return;
	}

	if (n_bytes_read == 0) {
		argv_size = 0;
		for (int i = argv_size; i < MAX_ARGS; i++) {
			free(argv[i]);
		}
		*argv = NULL;
	} else {

		char *end = cmd_buffer + n_bytes_read;
		int argc = 0;
		int letterCnt = 0;

		for (char *ptr = cmd_buffer; ptr < end; ptr++) {
			if (*ptr) {
				argv[argc][letterCnt] = *ptr;

				letterCnt++;
			} else {
				argv[argc][letterCnt] = '\0';

				letterCnt = 0;
				argc++;
			}

			if (ptr == end - 1) {
				for (int i = argc; i < MAX_ARGS; i++) {
					free(argv[i]);
				}
				argv_size = argc;
				*(argv + argc) = NULL;
				break;
			}
		}
	}

	close(cmd_fd);
}

void parse_envp(char **argv, char **vars, DIR *dir, struct dirent *ent) {

	sprintf(env_path, "/proc/%s/environ", ent->d_name);

	int env_fd = open(env_path, O_RDONLY);
	if (env_fd == -1) {
		report_error(env_path, -1);
		for (int i = 0; i < argv_size; i++) {
			free(argv[i]);
		}
		free(argv);

		for (int i = 0; i < MAX_VARS; i++) {
			free(vars[i]);
		}
		free(vars);

		return;
	}

	int n_bytes_read = read(env_fd, env_buffer, BUFF_SIZE);
	if (n_bytes_read == -1) {
		report_error(env_path, errno);

		for (int i = 0; i < argv_size; i++) {
			free(argv[i]);
		}
		free(argv);

		for (int i = 0; i < MAX_VARS; i++) {
			free(vars[i]);
		}
		free(vars);
		close(env_fd);
		closedir(dir);
		return;
	}

	if (n_bytes_read == 0) {
		vars_size = 0;
		for (int i = vars_size; i < MAX_VARS; i++) {
			free(vars[i]);
		}
		*vars = NULL;
	} else {

		char *end = env_buffer + n_bytes_read;
		int letterCnt = 0;
		int argc = 0;

		for (char *ptr = env_buffer; ptr < end; ptr++) {
			if (*ptr) {
				vars[argc][letterCnt] = *ptr;
				letterCnt++;
			} else {
				vars[argc][letterCnt] = '\0';
				letterCnt = 0;
				argc++;
			}

			if (ptr == end - 1) {
				for (int i = argc; i < MAX_VARS; i++) {
					free(vars[i]);
				}
				vars_size = argc;
				*(vars + argc) = NULL;
			}
		}
	}

	close(env_fd);
}

void ps(void) {
	DIR *dir = opendir("/proc");

	if (dir == NULL) {
		report_error("/proc", errno);
		return;
	}

	struct dirent *ent;
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

		char process_exe[PATH_MAX];
		char real_exe_path[PATH_MAX];

		sprintf(process_exe, "/proc/%s/exe", ent->d_name);
		if (!realpath(process_exe, real_exe_path)) {
			report_error(process_exe, errno);
			closedir(dir);
			return;
		}

		char **argv = calloc(MAX_ARGS, sizeof(char*));
		for (int i = 0; i < MAX_ARGS; i++)
			argv[i] = calloc(MAX_LINE_SIZE, sizeof(char));

		char **vars = calloc(MAX_VARS, sizeof(char*));
		for (int i = 0; i < MAX_VARS; i++)
			vars[i] = calloc(MAX_LINE_SIZE, sizeof(char));

		parse_argv(argv, vars, dir, ent);
		parse_envp(argv, vars, dir, ent);
		report_process(atoi(ent->d_name), real_exe_path, argv, vars);
		clean_up(argv, vars, argv_size, vars_size);
	}

	closedir(dir);
}
