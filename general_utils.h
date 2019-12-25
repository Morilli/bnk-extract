#ifndef _GENERAL_UTILS_H
#define _GENERAL_UTILS_H

#include <inttypes.h>
#include <stdbool.h>

int char2int(char input);

void hex2bytes(const char* input, uint8_t* output, int input_length);

int create_dir(char* path);

int create_dirs(char* dir_path, bool create_first, bool create_last);

#define dprintf(...) if (DEBUG) printf(__VA_ARGS__)
#define eprintf(...) fprintf(stderr, __VA_ARGS__)
#define vprintf(level, ...) if (VERBOSE >= level) printf(__VA_ARGS__)

#define max(a, b) __extension__ ({ \
    typeof(a) _a = (a); \
    typeof(b) _b = (b); \
    _a > _b ? _a : _b; \
})

#define min(a, b) __extension__ ({ \
    typeof(a) _a = (a); \
    typeof(b) _b = (b); \
    _a < _b ? _a : _b; \
})

#define LIST(type) struct { \
    uint32_t length; \
    uint32_t allocated_length; \
    type* objects; \
}

#define add_object(list, object) do { \
    if ((list)->allocated_length == 0) { \
        (list)->objects = malloc(16 * sizeof(typeof(*object))); \
        (list)->length = 0; \
        (list)->allocated_length = 16; \
    } else if ((list)->length == (list)->allocated_length) { \
        (list)->objects = realloc((list)->objects, ((list)->allocated_length + ((list)->allocated_length >> 1)) * sizeof(typeof(*object))); \
        (list)->allocated_length += (list)->allocated_length >> 1; \
    } \
 \
    (list)->objects[(list)->length] = *object; \
    (list)->length++; \
} while (0)

#define remove_object(list, index) do { \
    list->objects[index] = list->objects[list->length - 1]; \
    list->length--; \
    if (list->allocated_length != 16 && list->length == list->allocated_length >> 1) { \
        list->objects = realloc(list->objects, max((list->length + (list->length >> 1), 16) * sizeof(typeof(list->objects[0]))); \
    } \
} while (0)

#endif
