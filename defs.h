#ifndef _DEFS_H
#define _DEFS_H

#include <inttypes.h>

struct string_hash {
    char* string;
    uint32_t hash;
};

struct string_hashes {
    uint32_t amount;
    uint32_t allocated_amount;
    struct string_hash* pairs;
};

#endif
