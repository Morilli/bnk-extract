#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <assert.h>

#include "defs.h"
#include "general_utils.h"
#include "ww2ogg/api.h"
#include "revorb/api.h"

struct WPKFileEntry {
    uint32_t data_offset;
    uint32_t data_length;
    char* filename;
    uint8_t* data;
};

struct WPKFile {
    uint8_t magic[4];
    uint32_t version;
    uint32_t file_count;
    uint32_t offset_amount;
    uint32_t* offsets;
    struct WPKFileEntry* wpk_file_entries;
};


void parse_header(FILE* wpk_file, struct WPKFile* wpkfile)
{
    assert(fread(wpkfile->magic, 1, 4, wpk_file) == 4);
    assert(memcmp(wpkfile->magic, "r3d2", 4) == 0);
    assert(fread(&wpkfile->version, 4, 1, wpk_file) == 1);
    assert(fread(&wpkfile->file_count, 4, 1, wpk_file) == 1);
}

void parse_offsets(FILE* wpk_file, struct WPKFile* wpkfile)
{
    wpkfile->offsets = malloc(wpkfile->file_count * 4);
    wpkfile->offset_amount = wpkfile->file_count;
    assert(fread(wpkfile->offsets, 4, wpkfile->file_count, wpk_file) == wpkfile->file_count);
}

void parse_data(FILE* wpk_file, struct WPKFile* wpkfile)
{
    wpkfile->wpk_file_entries = malloc(wpkfile->file_count * sizeof(struct WPKFileEntry));
    for (uint32_t i = 0; i < wpkfile->offset_amount; i++) {
        fseek(wpk_file, wpkfile->offsets[i], SEEK_SET);

        assert(fread(&wpkfile->wpk_file_entries[i].data_offset, 4, 1, wpk_file) == 1);
        assert(fread(&wpkfile->wpk_file_entries[i].data_length, 4, 1, wpk_file) == 1);

        int filename_size;
        assert(fread(&filename_size, 4, 1, wpk_file) == 1);
        wpkfile->wpk_file_entries[i].filename = malloc(filename_size + 1);
        for (int j = 0; j < filename_size; j++) {
            wpkfile->wpk_file_entries[i].filename[j] = getc(wpk_file);
            fseek(wpk_file, 1, SEEK_CUR);
        }
        wpkfile->wpk_file_entries[i].filename[filename_size] = '\0';
        dprintf("string: \"%s\"\n", wpkfile->wpk_file_entries[i].filename);

        wpkfile->wpk_file_entries[i].data = malloc(wpkfile->wpk_file_entries[i].data_length);
        fseek(wpk_file, wpkfile->wpk_file_entries[i].data_offset, SEEK_SET);
        assert(fread(wpkfile->wpk_file_entries[i].data, 1, wpkfile->wpk_file_entries[i].data_length, wpk_file) == wpkfile->wpk_file_entries[i].data_length);
    }
}

void extract_wpk_file(char* wpk_path, StringHashes* string_hashes, char* output_path, bool wems_only, bool oggs_only)
{
    FILE* wpk_file = fopen(wpk_path, "rb");
    if (!wpk_file) {
        eprintf("Error: Failed to open \"%s\".\n", wpk_path);
        exit(EXIT_FAILURE);
    }

    struct WPKFile wpkfile;

    parse_header(wpk_file, &wpkfile);
    parse_offsets(wpk_file, &wpkfile);
    parse_data(wpk_file, &wpkfile);

    for (uint32_t i = 0; i < wpkfile.file_count; i++) {
        uint16_t string_index;
        for (string_index = 0; string_index < string_hashes->length; string_index++) {
            if (string_hashes->objects[string_index].hash == strtoul(wpkfile.wpk_file_entries[i].filename, NULL, 10))
                break;
        }
        int string_length = strlen(output_path) + strlen(wpkfile.wpk_file_entries[i].filename) + 2;
        if (string_index < string_hashes->length)
            string_length += strlen(string_hashes->objects[string_index].string) + 1;
        char cur_output_path[string_length];
        if (string_index < string_hashes->length)
            sprintf(cur_output_path, "%s/%s/%s", output_path, string_hashes->objects[string_index].string, wpkfile.wpk_file_entries[i].filename);
        else
            sprintf(cur_output_path, "%s/%s", output_path, wpkfile.wpk_file_entries[i].filename);
        create_dirs(cur_output_path, true, false);

        if (!oggs_only) {
            FILE* output_file = fopen(cur_output_path, "wb");
            if (!output_file) {
                eprintf("Error: Failed to open \"%s\"\n", cur_output_path);
                exit(EXIT_FAILURE);
            }
            vprintf(1, "Extracting \"%s\"\n", cur_output_path);
            fwrite(wpkfile.wpk_file_entries[i].data, 1, wpkfile.wpk_file_entries[i].data_length, output_file);
            fclose(output_file);
        }

        if (!wems_only) {
            // convert the .wem to .ogg
            char ogg_path[string_length];
            memcpy(ogg_path, cur_output_path, string_length);
            memcpy(&ogg_path[string_length - 5], ".ogg", 5);

            vprintf(1, "Extracting \"%s\"\n", ogg_path);
            BinaryData raw_wem = (BinaryData) {.length = wpkfile.wpk_file_entries[i].data_length, .data = wpkfile.wpk_file_entries[i].data};
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

        free(wpkfile.wpk_file_entries[i].filename);
        free(wpkfile.wpk_file_entries[i].data);
    }
    fclose(wpk_file);

    free(wpkfile.offsets);
    free(wpkfile.wpk_file_entries);
}
