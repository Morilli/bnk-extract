#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <assert.h>
#include "defs.h"
#include "bin.h"

uint32_t fnv_1_hash(const char* input)
{
    uint32_t hash = 0x811c9dc5;
    const char* c = input;
    while (*c != 0) {
        hash *= 0x01000193;
        hash ^= (*c > 64 && *c < 91) ? *c + 32 : *c;
        c++;
    }

    return hash;
}


StringHashes* parse_bin_file(char* bin_path)
{
    FILE* bin_file = fopen(bin_path, "rb");
    if (!bin_file) {
        eprintf("Error: Failed to open \"%s\".\n", bin_path);
        exit(EXIT_FAILURE);
    }

    StringHashes* saved_strings = calloc(1, sizeof(StringHashes));

    while (!feof(bin_file)) {
        if (getc(bin_file) == 0x84 && getc(bin_file) == 0xe3 && getc(bin_file) == 0xd8 && getc(bin_file) == 0x12) {
            fseek(bin_file, 6, SEEK_CUR);
            uint32_t amount;
            assert(fread(&amount, 4, 1, bin_file) == 1);
            dprintf("amount: %u\n", amount);
            uint32_t current_amount = saved_strings->length;
            saved_strings->length += amount;
            if (!saved_strings->objects)
                saved_strings->objects = malloc(amount * sizeof(struct string_hash));
            else
                saved_strings->objects = realloc(saved_strings->objects, saved_strings->length * sizeof(struct string_hash));

            for (; current_amount < saved_strings->length; current_amount++) {
                uint16_t length;
                assert(fread(&length, 2, 1, bin_file) == 1);
                saved_strings->objects[current_amount].string = malloc(length + 1);
                assert(fread(saved_strings->objects[current_amount].string, 1, length, bin_file) == length);
                saved_strings->objects[current_amount].string[length] = '\0';
                dprintf("saved string \"%s\"\n", saved_strings->objects[current_amount].string);
                saved_strings->objects[current_amount].hash = fnv_1_hash(saved_strings->objects[current_amount].string);
            }
        }
    }

    for (uint32_t i = 0; i < saved_strings->length; i++) {
        dprintf("string at position %u: \"%s\".\n", i, saved_strings->objects[i].string);
    }

    fclose(bin_file);
    return saved_strings;
}
