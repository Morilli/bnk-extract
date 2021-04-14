// #define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <assert.h>
#include "list.h"


#define _FILE_OFFSET_BITS 64

struct BNKFileEntry {
    uint32_t file_id;
    uint32_t offset;
    uint32_t length;
    uint8_t* data;
};

struct BNKFile {
    uint32_t length;
    uint32_t version;
    struct BNKFileEntry* entries;
};

struct wav_header {
    const char riff[4];
    uint32_t file_size;
    const char wave[4];
    const char fmt[4];
    const uint32_t fmt_length;
    const uint16_t fmt_tag;
    uint16_t channels;
    uint32_t sample_rate;
    uint32_t avg_bytes_per_second;
    uint16_t block_align;
    uint16_t bits_per_sample;
};

// note: ONLY DO THIS WITH POWERS OF 2
// clamps number to the next higher number that devides through this power of two, e.g. (1234, 8) -> 1240
#define clamp_int(number, clamp) (((clamp-1) + number) & ~(clamp-1))
// gives the distance to the next higher number that devides through this power of two, e.g. (1234, 8) -> 6
#define diff_clamp(number, clamp) ((clamp-1) - ((number+clamp-1) & (clamp-1)))

typedef LIST(char*) StringList;
typedef LIST(struct BNKFileEntry) BNKFileEntryList;

// https://stackoverflow.com/questions/1634359/is-there-a-reverse-function-for-strstr
char* rstrstr(const char* haystack, const char* needle)
{
    if (*needle == '\0')
        return (char*) haystack;

    char* result = NULL;
    while (1) {
        char* p = strstr(haystack, needle);
        if (p == NULL)
            break;
        result = p;
        haystack = p + 1;
    }

    return result;
}

StringList* files_in_directory(char* directory_path, StringList* files_list)
{
    int directory_path_length = strlen(directory_path);
    DIR* root_directory = opendir(directory_path);
    if (!root_directory) return NULL;

    struct dirent* current_direntry;
    struct stat temp_stat;
    // readdir
    while ( (current_direntry = readdir(root_directory)) != NULL) {
        if (strcmp(current_direntry->d_name, ".") == 0 || strcmp(current_direntry->d_name, "..") == 0)
            continue;

        char* current_path = malloc(directory_path_length + strlen(current_direntry->d_name) + 2);
        sprintf(current_path, "%s/%s", directory_path, current_direntry->d_name);
        stat(current_path, &temp_stat);
        // printf("current path: \"%s\"\n", current_path);
        if (S_ISDIR(temp_stat.st_mode)) {
            files_in_directory(current_path, files_list);
            free(current_path);
        } else {
            add_object(files_list, &current_path);
        }
    }

    closedir(root_directory);

    return files_list;
}

struct BNKFileEntry* wem_data_from_wav(char* wav_path)
{
    FILE* wav_file = fopen(wav_path, "rb");
    if (!wav_file) {
        eprintf("Error: Failed to open wav file \"%s\".\n", wav_path);
        return NULL;
    }

    struct BNKFileEntry* new_bnkfileentry = calloc(1, sizeof(struct BNKFileEntry));
    struct wav_header current_wav_header;
    assert(fread(&current_wav_header, sizeof(struct wav_header), 1, wav_file) == 1);

    new_bnkfileentry->length = current_wav_header.file_size + 8;
    new_bnkfileentry->data = malloc(new_bnkfileentry->length);


    // technically TODO: actually parse the wav contents and sections, and discard all trash
    memcpy(new_bnkfileentry->data, "RIFF", 4);
    memcpy(new_bnkfileentry->data + 4, &current_wav_header.file_size, 4);
    memcpy(new_bnkfileentry->data + 8, "WAVEfmt ", 8);
    memcpy(new_bnkfileentry->data + 16, &(uint32_t) {0x18}, 4);
    memcpy(new_bnkfileentry->data + 20, &(uint32_t) {0xFFFE}, 2);
    memcpy(new_bnkfileentry->data + 22, &current_wav_header.channels, 2);
    memcpy(new_bnkfileentry->data + 24, &current_wav_header.sample_rate, 4);
    memcpy(new_bnkfileentry->data + 28, &current_wav_header.avg_bytes_per_second, 4);
    memcpy(new_bnkfileentry->data + 32, &current_wav_header.block_align, 2);
    memcpy(new_bnkfileentry->data + 34, &current_wav_header.bits_per_sample, 2);
    memcpy(new_bnkfileentry->data + 36, &(uint16_t) {6}, 2);
    memcpy(new_bnkfileentry->data + 38, "\0\0\x01\x41\0\0", 6);
    assert(fread(new_bnkfileentry->data + 44, 4, 1, wav_file) == 1);
    uint32_t data_size;
    assert(fread(&data_size, 4, 1, wav_file) == 1);
    memcpy(new_bnkfileentry->data + 48, &data_size, 4);
    assert(fread(new_bnkfileentry->data + 52, data_size, 1, wav_file) == 1);

    // FILE* debug_output = fopen("__debug_testwavtowem", "wb");
    // fwrite(new_bnkfileentry->data, new_bnkfileentry->length, 1, debug_output);
    // fclose(debug_output);

    fclose(wav_file);

    return new_bnkfileentry;
}

void write_wpk_file(char* wpk_path, struct BNKFile* bnkfile, int clamp)
{
    assert(clamp && ((clamp & (clamp - 1)) == 0));
    FILE* wpk_file = fopen(wpk_path, "wb");
    if (!wpk_file) {
        eprintf("Error: Failed to open \"%s\".\n", wpk_path);
        exit(EXIT_FAILURE);
    }

    // write header
    fwrite("r3d2\1\0\0\0", 8, 1, wpk_file);
    fwrite(&bnkfile->length, 4, 1, wpk_file);


    // skip over the initial offset section and write them later
    fseek(wpk_file, bnkfile->length * 4, SEEK_CUR);
    fseek(wpk_file, diff_clamp(ftell(wpk_file), clamp), SEEK_CUR);

    char filename_string[15];
    for (uint32_t i = 0; i < bnkfile->length; i++) {
        // save offset to fill in the offset section later
        bnkfile->entries[i].offset = ftell(wpk_file);
        // printf("offset[%d]: %u\n", i, bnkfile->entries[i].offset);
        fseek(wpk_file, 4, SEEK_CUR);
        fwrite(&bnkfile->entries[i].length, 4, 1, wpk_file);
        sprintf(filename_string, "%u.wem", bnkfile->entries[i].file_id);
        int filename_string_length = strlen(filename_string);
        fwrite(&filename_string_length, 4, 1, wpk_file);
        for (int i = 0; i < filename_string_length; i++) {
            putc(filename_string[i], wpk_file);
            fseek(wpk_file, 1, SEEK_CUR);
        }
        fseek(wpk_file, diff_clamp(ftell(wpk_file), clamp), SEEK_CUR);
    }

    printf("this math , result for %d is %d, %d\n", 421, (421 + 7) & ~7, 7 - ((421 + 7) & 7));
    printf("this math , result for %d is %d, %d\n", 422, (422 + 7) & ~7, 7 - ((422 + 7) & 7));
    printf("this math , result for %d is %d, %d\n", 423, (423 + 7) & ~7, 7 - ((423 + 7) & 7));
    printf("this math , result for %d is %d, %d\n", 424, (424 + 7) & ~7, 7 - ((424 + 7) & 7));
    printf("this math , result for %d is %d, %d\n", 425, (425 + 7) & ~7, 7 - ((425 + 7) & 7));
    printf("this math , result for %d is %d, %d\n", 426, (426 + 7) & ~7, 7 - ((426 + 7) & 7));
    printf("this math , result for %d is %d, %d\n", 427, (427 + 7) & ~7, 7 - ((427 + 7) & 7));
    printf("this math , result for %d is %d, %d\n", 428, (428 + 7) & ~7, 7 - ((428 + 7) & 7));
    printf("this math , result for %d is %d, %d\n", 429, (429 + 7) & ~7, 7 - ((429 + 7) & 7));
    uint32_t start_data_offset = ftell(wpk_file);
    // printf("start_data_offset = %u\n", start_data_offset);
    for (uint32_t i = 0; i < bnkfile->length; i++) {
        // seek to initial place in the offset section and write offset
        // printf("seeking to %d and writing offset %u\n", 12 + 4*i, bnkfile->entries[i].offset);
        fseek(wpk_file, 12 + 4*i, SEEK_SET);
        fwrite(&bnkfile->entries[i].offset, 4, 1, wpk_file);

        // seek to the written offset, update the offset variable for further use and write data offset
        fseek(wpk_file, bnkfile->entries[i].offset, SEEK_SET);
        bnkfile->entries[i].offset = i == 0 ? start_data_offset : clamp_int(bnkfile->entries[i-1].offset + bnkfile->entries[i-1].length, clamp);
        // bnkfile->entries[i].offset = (bnkfile->entries[i].offset+7) & ~7;
        fwrite(&bnkfile->entries[i].offset, 4, 1, wpk_file);

        // seek to written data offset and write data
        fseek(wpk_file, bnkfile->entries[i].offset, SEEK_SET);
        fwrite(bnkfile->entries[i].data, bnkfile->entries[i].length, 1, wpk_file);
    }
}

void write_bnk_file(char* bnk_path, struct BNKFile* bnkfile, int clamp)
{
    assert(clamp && ((clamp & (clamp - 1)) == 0));
    FILE* bnk_file = fopen(bnk_path, "wb");
    if (!bnk_file) {
        eprintf("Error: Failed to open \"%s\".\n", bnk_path);
        exit(EXIT_FAILURE);
    }

    // write BKHD section
    fwrite("BKHD", 4, 1, bnk_file);
    uint8_t hardcoded[12] = "\0\0\0\0\xfa\0\0\0\0\0\0\0";
    uint32_t bkhd_section_length;
    if (bnkfile->version == 0x84) {
        bkhd_section_length = 0x18;
    } else if (bnkfile->version == 0x86) {
        bkhd_section_length = 0x14;
    }
    fwrite(&bkhd_section_length, 4, 1, bnk_file);
    fwrite(&bnkfile->version, 4, 1, bnk_file); // version
    fwrite("\xa3\xcd\x19\x03", 4, 1, bnk_file); // id of this bnk file, should ideally be given or taken from the original file
    fwrite("\x3e\x5d\x70\x17", 4, 1, bnk_file); // random hardcoded bytes?
    fwrite(hardcoded, bkhd_section_length - 12, 1, bnk_file);

    if (bnkfile->version != 0x86) {
        clamp = 0;
    }

    // write DIDX section
    fwrite("DIDX", 4, 1, bnk_file);
    fwrite(&(uint32_t) {bnkfile->length*12}, 4, 1, bnk_file);
    int initial_clamp_offset = ftell(bnk_file) + bnkfile->length*12 + 8;
    int total_offset = 0;
    printf("initial offset: %d\n", initial_clamp_offset);
    for (uint32_t i = 0; i < bnkfile->length; i++) {
        bnkfile->entries[i].offset += total_offset;
        int current_additional_offset = diff_clamp(initial_clamp_offset + bnkfile->entries[i].offset, clamp);
        bnkfile->entries[i].offset += current_additional_offset;
        total_offset += current_additional_offset;
        fwrite(&bnkfile->entries[i].file_id, 4, 1, bnk_file);
        fwrite(&bnkfile->entries[i].offset, 4, 1, bnk_file);
        fwrite(&bnkfile->entries[i].length, 4, 1, bnk_file);
    }

    // write DATA section
    fwrite("DATA", 4, 1, bnk_file);
    fwrite(&(uint32_t) {bnkfile->entries[bnkfile->length-1].offset + bnkfile->entries[bnkfile->length-1].length}, 4, 1, bnk_file);
    for (uint32_t i = 0; i < bnkfile->length; i++) {
        fwrite(bnkfile->entries[i].data, bnkfile->entries[i].length, 1, bnk_file);
        fseek(bnk_file, diff_clamp(ftell(bnk_file), clamp), SEEK_CUR);
    }
    fclose(bnk_file);
}

void create_bnk_from_folder(char* folder_path, char* bnk_path, int version, int clamp)
{
    struct BNKFile bnkfile = {0};
    bnkfile.version = version;
    BNKFileEntryList bnkentry_list;
    initialize_list(&bnkentry_list);
    StringList file_list;
    initialize_list(&file_list);
    files_in_directory(folder_path, &file_list);

    for (uint32_t i = 0; i < file_list.length; i++) {
        int filename_length = strlen(file_list.objects[i]);
        if (filename_length >= 4 && (strcmp(file_list.objects[i]+filename_length-4, ".wav") == 0 || strcmp(file_list.objects[i]+filename_length-4, ".wem") == 0)) {
            struct BNKFileEntry* to_find = NULL;
            uint32_t current_file_id = strtol(rstrstr(file_list.objects[i], "/") + 1, NULL, 10);
            find_object_s(&bnkentry_list, to_find, file_id, current_file_id);
            if (to_find) {
                continue; // prevent adding the same file_id multiple times
            }

            if (strcmp(file_list.objects[i]+filename_length-4, ".wav") == 0) {
                struct BNKFileEntry* new_bnkfileentry = wem_data_from_wav(file_list.objects[i]);
                new_bnkfileentry->file_id = strtol(rstrstr(file_list.objects[i], "/") + 1, NULL, 10);
                add_object_s(&bnkentry_list, new_bnkfileentry, file_id);
                free(new_bnkfileentry);
            } else {
                // printf("string here: \"%s\"\n", rstrstr(file_list.objects[i], "/") + 1);
                // printf("strtol of that: %ld\n", strtol(rstrstr(file_list.objects[i], "/") + 1, NULL, 10));
                struct BNKFileEntry new_bnkfileentry = {
                    .file_id = strtol(rstrstr(file_list.objects[i], "/") + 1, NULL, 10),
                };
                FILE* wem_file = fopen(file_list.objects[i], "rb");
                if (!wem_file) {
                    eprintf("Error: Failed to open wem file \"%s\".\n", file_list.objects[i]);
                    exit(EXIT_FAILURE);
                }
                fseek(wem_file, 0, SEEK_END);
                new_bnkfileentry.length = ftell(wem_file);
                new_bnkfileentry.data = malloc(new_bnkfileentry.length);
                rewind(wem_file);
                assert(fread(new_bnkfileentry.data, new_bnkfileentry.length, 1, wem_file) == 1);
                fclose(wem_file);
                add_object_s(&bnkentry_list, &new_bnkfileentry, file_id);
            }
        }
    }

    bnkfile.length = bnkentry_list.length;
    bnkfile.entries = bnkentry_list.objects;
    for (uint32_t i = 0; i < bnkfile.length; i++) {
        bnkfile.entries[i].offset = i == 0 ? 0 : bnkfile.entries[i-1].offset + bnkfile.entries[i-1].length;
        printf("bnkfile.entries[%d] fileid: %u, offset: %u\n", i, bnkfile.entries[i].file_id, bnkfile.entries[i].offset);
    }

    for (uint32_t i = 0; i < file_list.length; i++) {
        // printf("entry[%d] of file list: %s\n", i, file_list.objects[i]);
        free(file_list.objects[i]);
    }
    free(file_list.objects);

    int bnk_path_length = strlen(bnk_path);
    if (bnk_path_length >= 4 && strcmp(bnk_path + bnk_path_length - 4, ".bnk") == 0) {
        printf("Info: Writing .bnk format\n");
        write_bnk_file(bnk_path, &bnkfile, clamp == -1 ? 16 : clamp);
    } else {
        printf("Info: Writing .wpk format\n");
        write_wpk_file(bnk_path, &bnkfile, clamp == -1 ? 8 : clamp);
    }

    for (uint32_t i = 0; i < bnkfile.length; i++) {
        free(bnkfile.entries[i].data);
    }
    free(bnkfile.entries);
}


int main(int argc, char* argv[])
{
    if (argc < 3) {
        eprintf("sorry, please use 2 paths as argument (input folder, output bnk)\n");
        eprintf("optional arguments: version number in third position (i currently know 0x84 pre-11.8 and 0x86 after) and clamp amount in fourth (use a power of two, or best no value. this will cause byte aligns in bnk/wpk files to be different.\n");
        exit(EXIT_FAILURE);
    }

    int version = 0x86;
    int clamp = -1;
    if (argc >= 4) {
        version = strtol(argv[3], NULL, 0);
    }
    if (argc >= 5) {
        clamp = strtol(argv[4], NULL, 10);
    }

    create_bnk_from_folder(argv[1], argv[2], version, clamp);
}
