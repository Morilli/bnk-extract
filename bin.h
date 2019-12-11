#ifndef _BIN_H
#define _BIN_H

#include <inttypes.h>
#include "defs.h"

uint32_t fnv_1_hash(const char* input);
struct string_hashes* parse_bin_file(char* bin_path);

#endif
