#ifndef _GENERAL_UTILS_H
#define _GENERAL_UTILS_H

#include <inttypes.h>
#include <stdbool.h>

int char2int(char input);

void hex2bytes(const char* input, uint8_t* output, int input_length);

int create_dir(char* path);

int create_dirs(char* dir_path, bool create_first, bool create_last);

#endif
