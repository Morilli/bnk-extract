#ifndef _DEFS_H
#define _DEFS_H

#include <inttypes.h>
#include "general_utils.h"

#define DEBUG 0
int VERBOSE;

struct string_hash {
    char* string;
    uint32_t hash;
};

typedef LIST(struct string_hash) StringHashes;

#endif
