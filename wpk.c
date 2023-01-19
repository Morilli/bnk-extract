#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <assert.h>

#include "wpk.h"
#include "defs.h"
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
    int real_file_count = 0;
    for (uint32_t i = 0; i < wpkfile->offset_amount; i++) {
        if (wpkfile->offsets[i] != 0) // riot with their padding bytes :)
            real_file_count++;
    }
    wpkfile->file_count = real_file_count;

    wpkfile->wpk_file_entries = malloc(wpkfile->file_count * sizeof(struct WPKFileEntry));
    int real_index = 0;
    for (uint32_t i = 0; i < wpkfile->offset_amount; i++, real_index++) {
        if (wpkfile->offsets[i] == 0) {
            real_index--;
            continue;
        }
        fseek(wpk_file, wpkfile->offsets[real_index], SEEK_SET);

        assert(fread(&wpkfile->wpk_file_entries[real_index].data_offset, 4, 1, wpk_file) == 1);
        assert(fread(&wpkfile->wpk_file_entries[real_index].data_length, 4, 1, wpk_file) == 1);

        int filename_size;
        assert(fread(&filename_size, 4, 1, wpk_file) == 1);
        wpkfile->wpk_file_entries[real_index].filename = malloc(filename_size + 1);
        for (int j = 0; j < filename_size; j++) {
            wpkfile->wpk_file_entries[real_index].filename[j] = getc(wpk_file);
            fseek(wpk_file, 1, SEEK_CUR);
        }
        wpkfile->wpk_file_entries[real_index].filename[filename_size] = '\0';
        dprintf("string: \"%s\"\n", wpkfile->wpk_file_entries[real_index].filename);

        wpkfile->wpk_file_entries[real_index].data = malloc(wpkfile->wpk_file_entries[real_index].data_length);
        fseek(wpk_file, wpkfile->wpk_file_entries[real_index].data_offset, SEEK_SET);
        assert(fread(wpkfile->wpk_file_entries[real_index].data, 1, wpkfile->wpk_file_entries[real_index].data_length, wpk_file) == wpkfile->wpk_file_entries[real_index].data_length);
    }
}

void extract_wpk_file(char* wpk_path, StringHashes* string_hashes, char* output_path, bool wems_only, bool oggs_only, bool alternate_filenames)
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
    fclose(wpk_file);

    AudioDataList audio_data_list;
    initialize_list_size(&audio_data_list, wpkfile.file_count);
    for (uint32_t i = 0; i < wpkfile.file_count; i++) {
        add_object(&audio_data_list, (&(AudioData) {.id = strtoul(wpkfile.wpk_file_entries[i].filename, NULL, 10), .data = {.length = wpkfile.wpk_file_entries[i].data_length, .data = wpkfile.wpk_file_entries[i].data}}));
    }

    extract_all_audio(output_path, &audio_data_list, string_hashes, wems_only, oggs_only, alternate_filenames);

    for (uint32_t i = 0; i < wpkfile.file_count; i++) {
        free(wpkfile.wpk_file_entries[i].filename);
        free(wpkfile.wpk_file_entries[i].data);
    }
    free(wpkfile.offsets);
    free(wpkfile.wpk_file_entries);
    free(audio_data_list.objects);
}
