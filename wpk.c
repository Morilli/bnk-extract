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
    for (uint32_t i = 0; i < wpkfile->file_count; i++) {
        assert(fread(&wpkfile->offsets[i], 4, 1, wpk_file) == 1);
    }
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
            wpkfile->wpk_file_entries[i].filename[j] = fgetc(wpk_file);
            fseek(wpk_file, 1, SEEK_CUR);
        }
        wpkfile->wpk_file_entries[i].filename[filename_size] = '\0';
        printf("string: \"%s\"\n", wpkfile->wpk_file_entries[i].filename);

        wpkfile->wpk_file_entries[i].data = malloc(wpkfile->wpk_file_entries[i].data_length);
        fseek(wpk_file, wpkfile->wpk_file_entries[i].data_offset, SEEK_SET);
        assert(fread(wpkfile->wpk_file_entries[i].data, 1, wpkfile->wpk_file_entries[i].data_length, wpk_file) == wpkfile->wpk_file_entries[i].data_length);
    }
}

void extract_wpk_file(char* wpk_path, struct string_hashes* string_hashes, char* output_path, bool delete_wems)
{
    FILE* wpk_file = fopen(wpk_path, "rb");
    if (!wpk_file) {
        fprintf(stderr, "Error: Failed to open \"%s\".\n", wpk_path);
        exit(EXIT_FAILURE);
    }

    struct WPKFile wpkfile;

    parse_header(wpk_file, &wpkfile);
    parse_offsets(wpk_file, &wpkfile);
    parse_data(wpk_file, &wpkfile);

    for (uint32_t i = 0; i < wpkfile.file_count; i++) {
        uint16_t string_index;
        for (string_index = 0; string_index < string_hashes->amount; string_index++) {
            if (string_hashes->pairs[string_index].hash == strtoul(wpkfile.wpk_file_entries[i].filename, NULL, 10))
                break;
        }
        int string_length = strlen(output_path) + strlen(wpkfile.wpk_file_entries[i].filename) + 2;
        if (string_index < string_hashes->amount)
            string_length += strlen(string_hashes->pairs[string_index].string) + 1;
        char cur_output_path[string_length];
        if (string_index < string_hashes->amount)
            sprintf(cur_output_path, "%s/%s/%s", output_path, string_hashes->pairs[string_index].string, wpkfile.wpk_file_entries[i].filename);
        else
            sprintf(cur_output_path, "%s/%s", output_path, wpkfile.wpk_file_entries[i].filename);
        create_dirs(cur_output_path, true, false);
        FILE* output_file = fopen(cur_output_path, "wb");
        if (!output_file) {
            fprintf(stderr, "Error: Failed to open \"%s\"\n", cur_output_path);
            exit(EXIT_FAILURE);
        }

        fwrite(wpkfile.wpk_file_entries[i].data, 1, wpkfile.wpk_file_entries[i].data_length, output_file);
        fclose(output_file);

        // convert the .wem to .ogg
        char ogg_path[string_length];
        memcpy(ogg_path, cur_output_path, string_length);
        memcpy(&ogg_path[string_length - 5], ".ogg", 5);
        char* ww2ogg_args[3] = {"", cur_output_path};
        ww2ogg(sizeof(ww2ogg_args) / sizeof(ww2ogg_args[0]) - 1, ww2ogg_args);
        const char* revorb_args[3] = {"", ogg_path};
        revorb(sizeof(revorb_args) / sizeof(revorb_args[0]) - 1, revorb_args);

        if (delete_wems)
            remove(cur_output_path);
    }
    fclose(wpk_file);

    for (uint32_t i = 0; i < wpkfile.file_count; i++) {
        free(wpkfile.wpk_file_entries[i].filename);
        free(wpkfile.wpk_file_entries[i].data);
    }
    free(wpkfile.offsets);
    free(wpkfile.wpk_file_entries);
}
