#include <stdbool.h>
#include "defs.h"
#include "bin.h"
#include "list.h"

typedef struct audio_data {
    uint32_t id;
    BinaryData data;
} AudioData;

typedef LIST(AudioData) AudioDataList;

void extract_all_audio(char* output_path, AudioDataList* audio_data_list, StringHashes* string_hashes, bool wems_only, bool oggs_onlys, bool alternate_filenames);
