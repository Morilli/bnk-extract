#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "defs.h"
#include "general_utils.h"
#include "ww2ogg/api.h"
#include "revorb/api.h"

void extract_audio(char* output_path, BinaryData* wem_data, bool wem_only, bool ogg_only)
{
    create_dirs(output_path, false);

    if (!ogg_only) {
        FILE* output_file = fopen(output_path, "wb");
        if (!output_file) {
            eprintf("Error: Failed to open \"%s\"\n", output_path);
            exit(EXIT_FAILURE);
        }
        v_printf(1, "Extracting \"%s\"\n", output_path);
        fwrite(wem_data->data, 1, wem_data->length, output_file);
        fclose(output_file);
    }

    if (!wem_only) {
        // convert the .wem to .ogg
        int string_length = strlen(output_path) + 1;
        char ogg_path[string_length];
        memcpy(ogg_path, output_path, string_length);
        memcpy(&ogg_path[string_length - 5], ".ogg", 5);

        v_printf(1, "Extracting \"%s\"\n", ogg_path);
        char data_pointer[17] = {0};
        bytes2hex(&wem_data, data_pointer, 8);
        char* ww2ogg_args[4] = {"", "--binarydata", data_pointer};
        BinaryData* raw_ogg = ww2ogg(sizeof(ww2ogg_args) / sizeof(ww2ogg_args[0]) - 1, ww2ogg_args);
        if (!raw_ogg)
            return;

        bytes2hex(&raw_ogg, data_pointer, 8);
        const char* revorb_args[4] = {"", data_pointer, ogg_path};
        revorb(sizeof(revorb_args) / sizeof(revorb_args[0]) - 1, revorb_args);
        free(raw_ogg->data);
        free(raw_ogg);
    }
}
