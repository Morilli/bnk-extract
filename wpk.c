#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <assert.h>

#include "defs.h"
#include "general_utils.h"
#include "bin.h"
#include "extract.h"

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
        bool extracted = false;
        for (uint32_t string_index = 0; string_index < string_hashes->length; string_index++) {
            if (string_hashes->objects[string_index].hash == strtoul(wpkfile.wpk_file_entries[i].filename, NULL, 10)) {
                extracted = true;
                char cur_output_path[strlen(output_path) + strlen(string_hashes->objects[string_index].string) + strlen(wpkfile.wpk_file_entries[i].filename) + 3];
                if (string_hashes->objects[string_index].switch_id)
                    sprintf(cur_output_path, "%s/%s/%u/%s", output_path, string_hashes->objects[string_index].string, string_hashes->objects[string_index].switch_id, wpkfile.wpk_file_entries[i].filename);
                else
                    sprintf(cur_output_path, "%s/%s/%s", output_path, string_hashes->objects[string_index].string, wpkfile.wpk_file_entries[i].filename);
                // sprintf(cur_output_path, "%s/%s/%s", output_path, string_hashes->objects[string_index].string, wpkfile.wpk_file_entries[i].filename);

                extract_audio(cur_output_path, &(BinaryData) {.length = wpkfile.wpk_file_entries[i].data_length, .data = wpkfile.wpk_file_entries[i].data}, wems_only, oggs_only);
            }
        }
        if (!extracted) {
            char cur_output_path[strlen(output_path) + strlen(wpkfile.wpk_file_entries[i].filename) + 2];
            sprintf(cur_output_path, "%s/%s", output_path, wpkfile.wpk_file_entries[i].filename);
            extract_audio(cur_output_path, &(BinaryData) {.length = wpkfile.wpk_file_entries[i].data_length, .data = wpkfile.wpk_file_entries[i].data}, wems_only, oggs_only);
        }
        free(wpkfile.wpk_file_entries[i].filename);
        free(wpkfile.wpk_file_entries[i].data);
    }
    fclose(wpk_file);

    free(wpkfile.offsets);
    free(wpkfile.wpk_file_entries);
}
