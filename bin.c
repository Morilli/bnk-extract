#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "defs.h"

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


struct string_hashes* parse_bin_file(char* bin_path)
{
    FILE* bin_file = fopen(bin_path, "rb");
    if (!bin_file) {
        fprintf(stderr, "Error: Failed to open \"%s\".\n", bin_path);
        exit(EXIT_FAILURE);
    }

    struct string_hashes* saved_strings = calloc(1, sizeof(struct string_hashes));

    while (!feof(bin_file)) {
        if (getc(bin_file) == 0x84 && getc(bin_file) == 0xe3 && getc(bin_file) == 0xd8 && getc(bin_file) == 0x12) {
            fseek(bin_file, 6, SEEK_CUR);
            uint32_t amount;
            fread(&amount, 4, 1, bin_file);
            printf("amount: %u\n", amount);
            uint32_t current_amount = saved_strings->amount;
            saved_strings->amount += amount;
            if (!saved_strings->pairs)
                saved_strings->pairs = malloc(amount * sizeof(struct string_hash));
            else
                saved_strings->pairs = realloc(saved_strings->pairs, saved_strings->amount * sizeof(struct string_hash));

            for (; current_amount < saved_strings->amount; current_amount++) {
                uint16_t length;
                fread(&length, 2, 1, bin_file);
                saved_strings->pairs[current_amount].string = malloc(length + 1);
                fread(saved_strings->pairs[current_amount].string, 1, length, bin_file);
                saved_strings->pairs[current_amount].string[length] = '\0';
                printf("saved string \"%s\"\n", saved_strings->pairs[current_amount].string);
                saved_strings->pairs[current_amount].hash = fnv_1_hash(saved_strings->pairs[current_amount].string);
            }
        }
    }

    for (uint32_t i = 0; i < saved_strings->amount; i++) {
        printf("string at position %u: \"%s\".\n", i, saved_strings->pairs[i].string);
    }

    fclose(bin_file);
    return saved_strings;
}
