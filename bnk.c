#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stdbool.h>
#include <assert.h>

#include "defs.h"
#include "general_utils.h"
#include "revorb/api.h"
#include "ww2ogg/api.h"

struct BNKFileEntry {
    uint32_t file_id;
    uint32_t offset;
    uint32_t length;
    uint8_t* data;
};

struct BNKFile {
    uint32_t length;
    struct BNKFileEntry* entries;
};

uint32_t skip_to_section(FILE* bnk_file, char name[4], bool from_beginning)
{
    if (from_beginning)
        fseek(bnk_file, 0, SEEK_SET);

    uint8_t header[4];
    uint32_t section_length = 0;

    do {
        fseek(bnk_file, section_length, SEEK_CUR);
        assert(fread(header, 1, 4, bnk_file) == 4);
        assert(fread(&section_length, 4, 1, bnk_file) == 1);
    } while (memcmp(header, name, 4) != 0);

    return section_length;
}

void parse_bnk_file_entries(FILE* bnk_file, struct BNKFile* bnkfile)
{
    uint32_t section_length = skip_to_section(bnk_file, "DIDX", false);

    bnkfile->length = section_length / 12;
    bnkfile->entries = malloc(bnkfile->length * sizeof(struct BNKFileEntry));
    for (uint32_t i = 0; i < bnkfile->length; i++) {
        assert(fread(&bnkfile->entries[i].file_id, 4, 1, bnk_file) == 1);
        assert(fread(&bnkfile->entries[i].offset, 4, 1, bnk_file) == 1);
        assert(fread(&bnkfile->entries[i].length, 4, 1, bnk_file) == 1);
    }

    section_length = skip_to_section(bnk_file, "DATA", false);

    uint32_t data_offset = ftell(bnk_file);
    for (uint32_t i = 0; i < bnkfile->length; i++) {
        fseek(bnk_file, data_offset + bnkfile->entries[i].offset, SEEK_SET);
        bnkfile->entries[i].data = malloc(bnkfile->entries[i].length);
        assert(fread(bnkfile->entries[i].data, 1, bnkfile->entries[i].length, bnk_file) == bnkfile->entries[i].length);
    }
}

void extract_bnk_file(char* bnk_path, StringHashes* string_hashes, char* output_path, bool wems_only, bool oggs_only)
{
    FILE* bnk_file = fopen(bnk_path, "rb");
    if (!bnk_file) {
        eprintf("Error: Failed to open \"%s\".\n", bnk_path);
        exit(EXIT_FAILURE);
    }

    struct BNKFile bnkfile;

    parse_bnk_file_entries(bnk_file, &bnkfile);

    for (uint32_t i = 0; i < bnkfile.length; i++) {
        uint16_t string_index;
        for (string_index = 0; string_index < string_hashes->length; string_index++) {
            if (string_hashes->objects[string_index].hash == bnkfile.entries[i].file_id)
                break;
        }
        char filename_string[15];
        sprintf(filename_string, "%u.wem", bnkfile.entries[i].file_id);
        int string_length = strlen(output_path) + strlen(filename_string) + 2;
        if (string_index < string_hashes->length)
            string_length += strlen(string_hashes->objects[string_index].string) + 1;
        char cur_output_path[string_length];
        if (string_index < string_hashes->length)
            sprintf(cur_output_path, "%s/%s/%s", output_path, string_hashes->objects[string_index].string, filename_string);
        else
            sprintf(cur_output_path, "%s/%s", output_path, filename_string);
        create_dirs(cur_output_path, true, false);

        if (!oggs_only) {
            FILE* output_file = fopen(cur_output_path, "wb");
            if (!output_file) {
                eprintf("Error: Failed to open \"%s\"\n", cur_output_path);
                exit(EXIT_FAILURE);
            }
            vprintf(1, "Extracting \"%s\"\n", cur_output_path);
            fwrite(bnkfile.entries[i].data, 1, bnkfile.entries[i].length, output_file);
            fclose(output_file);
        }

        if (!wems_only) {
            // convert the .wem to .ogg
            char ogg_path[string_length];
            memcpy(ogg_path, cur_output_path, string_length);
            memcpy(&ogg_path[string_length - 5], ".ogg", 5);

            vprintf(1, "Extracting \"%s\"\n", ogg_path);
            BinaryData raw_wem = (BinaryData) {.length = bnkfile.entries[i].length, .data = bnkfile.entries[i].data};
            char data_pointer[17] = {0};

            bytes2hex(&(void*) {&raw_wem}, data_pointer, 8);
            char* ww2ogg_args[4] = {"", "--binarydata", data_pointer};
            BinaryData* raw_ogg = ww2ogg(sizeof(ww2ogg_args) / sizeof(ww2ogg_args[0]) - 1, ww2ogg_args);

            bytes2hex(&raw_ogg, data_pointer, 8);
            const char* revorb_args[4] = {"", data_pointer, ogg_path};
            revorb(sizeof(revorb_args) / sizeof(revorb_args[0]) - 1, revorb_args);
            free(raw_ogg->data);
            free(raw_ogg);
        }

        free(bnkfile.entries[i].data);
    }
}
