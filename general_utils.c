#ifndef _WIN32
#   include <sys/stat.h>
#else
#   include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>

int char2int(char input)
{
    if (input >= '0' && input <= '9')
        return input - '0';
    if (input >= 'A' && input <= 'F')
        return input - 'A' + 10;
    if (input >= 'a' && input <= 'f')
        return input - 'a' + 10;
    fprintf(stderr, "Error: Malformed input to \"%s\" (%c).\n", __func__, input);
    return -1;
}

void hex2bytes(const char* input, uint8_t* output, int input_length)
{
    for (int i = 0; i < input_length; i += 2) {
        output[i / 2] = char2int(input[i]) * 16 + char2int(input[i + 1]);
    }
}


int create_dir(char* path)
{
    #ifdef _WIN32
        int ret = mkdir(path);
    #else
        int ret = mkdir(path, 0700);
    #endif
    if (ret == -1 && errno != EEXIST)
        return -1;
    return 0;
}

int create_dirs(char* dir_path, bool create_first, bool create_last)
{
    char* c = dir_path;
    bool skip_once = !create_first;
    while (*c != 0) {
        c++;
        if (*c == '/' || *c == '\\') {
            if (skip_once) {
                skip_once = false;
                continue;
            }
            char _c = *c;
            *c = '\0';
            if (create_dir(dir_path) == -1)
                return -1;
            *c = _c;
        }
    }
    if (create_last && create_dir(dir_path) == -1)
        return -1;
    return 0;
}
