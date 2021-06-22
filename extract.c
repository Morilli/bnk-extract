#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#ifdef _WIN32
#   include <windows.h>
#else
#   include <unistd.h>
#endif

#include "bin.h"
#include "extract.h"
#include "defs.h"
#include "general_utils.h"
#include "ww2ogg/api.h"
#include "revorb/api.h"

void hardlink_file(char* existing_path, char* new_path, uint8_t type)
{
    if (type & 1) {
        if (link(existing_path, new_path) != 0 && errno != EEXIST) {
            eprintf("Error: Failed hard-linking \"%s\" to \"%s\"\n", new_path, existing_path);
            return;
        }
    }
    if (type <= 1) return;

    int existing_path_length = strlen(existing_path);
    int new_path_length = strlen(new_path);
    char converted_existing_path[existing_path_length+1], converted_new_path[new_path_length+1];
    memcpy(converted_existing_path, existing_path, existing_path_length);
    memcpy(converted_existing_path + existing_path_length - 4, type & 2 ? ".ogg" : ".wav", 5);
    memcpy(converted_new_path, new_path, new_path_length);
    memcpy(converted_new_path + new_path_length - 4, type & 2 ? ".ogg" : ".wav", 5);
    if (link(converted_existing_path, converted_new_path) != 0 && errno != EEXIST) {
        eprintf("Error: Failed hard-linking \"%s\" to \"%s\"\n", converted_new_path, converted_existing_path);
        return;
    }
}

uint8_t extract_audio(char* output_path, BinaryData* wem_data, bool wem_only, bool ogg_only)
{
    uint8_t type = 0;
    if (!ogg_only) {
        FILE* output_file = fopen(output_path, "wb");
        if (!output_file) {
            eprintf("Error: Failed to open \"%s\"\n", output_path);
            return type;
        }
        v_printf(1, "Extracting \"%s\"\n", output_path);
        fwrite(wem_data->data, 1, wem_data->length, output_file);
        fclose(output_file);
        type |= 1;
    }

    if (!wem_only) {
        // convert the .wem to .ogg
        int string_length = strlen(output_path) + 1;
        char ogg_path[string_length];
        memcpy(ogg_path, output_path, string_length);
        ogg_path[string_length - 5] = '\0';

        v_printf(1, "Extracting \"%s", ogg_path);
        fflush(stdout);
        char data_pointer[17] = {0};
        bytes2hex(&wem_data, data_pointer, 8);
        char* ww2ogg_args[] = {"", "--binarydata", data_pointer, NULL};
        BinaryData* raw_ogg = ww2ogg(sizeof(ww2ogg_args) / sizeof(ww2ogg_args[0]) - 1, ww2ogg_args);
        if (!raw_ogg)
            return type;
        if (memcmp(raw_ogg->data, "RIFF", 4) == 0) { // got a wav file instead of an ogg one
            memcpy(&ogg_path[string_length - 5], ".wav", 5);
            FILE* wav_file = fopen(ogg_path, "wb");
            if (!wav_file) {
                eprintf("Could not open output file.\n");
                goto end;
            }
            fwrite(raw_ogg->data, raw_ogg->length, 1, wav_file);
            fclose(wav_file);
            type |= 4;
        } else {
            memcpy(&ogg_path[string_length - 5], ".ogg", 5);
            bytes2hex(&raw_ogg, data_pointer, 8);
            const char* revorb_args[] = {"", data_pointer, ogg_path, NULL};
            if (revorb(sizeof(revorb_args) / sizeof(revorb_args[0]) - 1, revorb_args) != 0) {
                goto end;
            }
            type |= 2;
        }
        end:
        free(raw_ogg->data);
        free(raw_ogg);
    }

    return type;
}

void extract_all_audio(char* output_path, AudioDataList* audio_data, StringHashes* string_hashes, bool wems_only, bool oggs_only)
{
    create_dirs(output_path, true);
    int output_path_length = strlen(output_path);

    for (uint32_t i = 0; i < audio_data->length; i++) {
        bool extracted = false;
        uint8_t extracted_type;
        char* initial_output_path;
        for (uint32_t string_index = 0; string_index < string_hashes->length; string_index++) {
            if (string_hashes->objects[string_index].hash == audio_data->objects[i].id) {
                char cur_output_path[output_path_length + strlen(string_hashes->objects[string_index].string) + 39];
                int current_position = output_path_length;
                strcpy(cur_output_path, output_path);
                current_position += sprintf(cur_output_path + output_path_length, "/%s", string_hashes->objects[string_index].string);
                if (string_hashes->objects[string_index].container_id)
                    current_position += sprintf(cur_output_path + current_position, "/%u", string_hashes->objects[string_index].container_id);
                if (string_hashes->objects[string_index].music_segment_id)
                    current_position += sprintf(cur_output_path + current_position, "/%u", string_hashes->objects[string_index].music_segment_id);
                create_dirs(cur_output_path, true);
                sprintf(cur_output_path + current_position, "/%u.wem", audio_data->objects[i].id);

                if (extracted) {
                    hardlink_file(initial_output_path, cur_output_path, extracted_type);
                } else {
                    extracted = true;
                    extracted_type = extract_audio(cur_output_path, &audio_data->objects[i].data, wems_only, oggs_only);
                    initial_output_path = strdup(cur_output_path);
                }
            }
        }
        if (!extracted) {
            char cur_output_path[output_path_length + 16];
            sprintf(cur_output_path, "%s/%u.wem", output_path, audio_data->objects[i].id);
            extract_audio(cur_output_path, &audio_data->objects[i].data, wems_only, oggs_only);
        } else {
            free(initial_output_path);
        }
    }
}
