#ifndef _WIN32
#   include <sys/stat.h>
#else
#   include <unistd.h>
#endif
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <inttypes.h>
#include <ctype.h>
#include <errno.h>

#include "general_utils.h"
#include "defs.h"

char* lower_inplace(char* string)
{
    for (char* c = string; *c; c++) {
        *c = tolower(*c);
    }

    return string;
}

char* lower(const char* string)
{
    size_t string_length = strlen(string);
    char* lower_string = malloc(string_length + 1);
    for (size_t i = 0; i < string_length; i++) {
        lower_string[i] = tolower(string[i]);
    }
    lower_string[string_length] = '\0';

    return lower_string;
}

int char2int(char input)
{
    if (input >= '0' && input <= '9')
        return input - '0';
    if (input >= 'A' && input <= 'F')
        return input - 'A' + 10;
    if (input >= 'a' && input <= 'f')
        return input - 'a' + 10;
    eprintf("Error: Malformed input to \"%s\" (%X).\n", __func__, input);
    return -1;
}

void hex2bytes(const char* input, void* output, int input_length)
{
    for (int i = 0; i < input_length; i += 2) {
        ((uint8_t*) output)[i / 2] = char2int(input[i]) * 16 + char2int(input[i + 1]);
    }
}

// converts input_length bytes from input into uppercase hexadecimal representation and saves them in output.
// Will not modify input, add a null byte at the end of output, or allocate the output buffer (make sure it's big enough)
void bytes2hex(const void* input, char* output, int input_length)
{
    const uint8_t* in = input;
    for (int i = 0; i < input_length; i++) {
        output[2 * i] = ((in[i] & 0xF0) >> 4) + (in[i] > 159 ? 55 : 48);
        output[2 * i + 1] = (in[i] & 0x0F) + ((in[i] & 0x0F) > 9 ? 55 : 48);
    }
}

int create_dir(char* path)
{
    int ret = mkdir(path, 0700);
    if (ret == -1 && errno != EEXIST)
        return -1;
    return 0;
}

int create_dirs(char* dir_path, bool create_last)
{
    char* c = dir_path;
    if (strlen(c) >= 3 && c[1] == ':' && (c[2] == '\\' || c[2] == '/'))
        c += 3;
    while (*c != 0) {
        c++;
        if (*c == '/' || *c == '\\') {
            char _c = *c;
            *c = '\0';
            int ret = create_dir(dir_path);
            *c = _c;
            if (ret == -1)
                return -1;
        }
    }
    if (create_last && create_dir(dir_path) == -1)
        return -1;
    return 0;
}
