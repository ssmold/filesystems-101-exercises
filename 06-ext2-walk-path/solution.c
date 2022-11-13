#include <solution.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


char* get_next_dir_name(const char* ptr, unsigned* length, const char** name) {
    char* endPtr = strpbrk(ptr + 1, "/");
    *length = endPtr - ptr - 1;
    const char* dirName = ptr + 1;
    *name = dirName;
    return endPtr;
}


int dump_file(int img, const char *path, int out) {
	(void) img;
	(void) path;
	(void) out;

    const char* charPtr = path;
    const char** dirName;
    unsigned length = 0;
    char* name = (char *)malloc(sizeof(char) * 256);

    while ((charPtr = get_next_dir_name(charPtr, &length, dirName))) {
        stpncpy(name, *dirName, length);
        name[length] = '\0';

    }

    free(name);
    return 0;
}
