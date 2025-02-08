#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>

#include "defs.h"
#include "bin.h"
#include "bnk.h"
#include "wpk.h"

int VERBOSE = 0;

// http://wiki.xentax.com/index.php/Wwise_SoundBank_(*.bnk)

// almost full documentation of all types and versions: https://github.com/bnnm/wwiser

struct sound {
    uint32_t self_id;
    uint32_t file_id;
    uint32_t source_id;
    uint32_t sound_object_id;
    uint8_t is_streamed;
};

struct music_track {
    uint32_t self_id;
    uint32_t file_id;
    uint32_t music_container_id;
};

struct music_container {
    uint32_t self_id;
    uint32_t music_switch_id;
    uint32_t sound_object_id;
    uint32_t music_track_id_amount;
    uint32_t* music_track_ids;
};

struct music_switch {
    uint32_t self_id;
    uint32_t some_id;
};

struct event_action {
    uint32_t self_id;
    uint8_t scope;
    uint8_t type;
    union {
        uint32_t sound_object_id;
        uint32_t switch_group_id;
    };
};

struct event {
    uint32_t self_id;
    uint8_t event_amount;
    uint32_t* event_ids;
};

struct random_container {
    uint32_t self_id;
    uint32_t switch_container_id;
    uint32_t sound_id_amount;
    uint32_t* sound_ids;
};

typedef LIST(struct sound) SoundSection;
typedef LIST(struct music_track) MusicTrackSection;
typedef LIST(struct music_container) MusicContainerSection;
typedef LIST(struct event_action) EventActionSection;
typedef LIST(struct event) EventSection;
typedef LIST(struct random_container) RandomContainerSection;
typedef LIST(struct music_switch) MusicSwitchSection;

void skip_initial_fx_params(FILE* bnk_file, uint32_t bnk_version)
{
    fseek(bnk_file, 1, SEEK_CUR);
    uint8_t num_fx = getc(bnk_file);
    if (num_fx) fseek(bnk_file, 1, SEEK_CUR);
    fseek(bnk_file, num_fx * (bnk_version <= 0x91 ? 7 : 6), SEEK_CUR);
}

void skip_initial_params(FILE* bnk_file)
{
    uint8_t prop_count = getc(bnk_file);
    fseek(bnk_file, 5 * prop_count, SEEK_CUR);
    prop_count = getc(bnk_file);
    fseek(bnk_file, 9 * prop_count, SEEK_CUR);
}

void skip_positioning_params(FILE* bnk_file, uint32_t bnk_version)
{
    uint8_t positioning_bits = getc(bnk_file);
    bool has_positioning = positioning_bits & 1, has_3d = false, has_automation = false;
    if (has_positioning) {
        if (bnk_version <= 0x59) {
            bool has_2d = getc(bnk_file);
            has_3d = getc(bnk_file);
            if (has_2d) getc(bnk_file);
        } else {
            has_3d = positioning_bits & 0x2;
        }
    }
    if (has_positioning && has_3d) {
        if (bnk_version <= 0x59) {
            has_automation = (getc(bnk_file) & 3) != 1;
            fseek(bnk_file, 8, SEEK_CUR);
        } else {
            has_automation = (positioning_bits >> 5) & 3;
            getc(bnk_file);
        }
    }
    if (has_automation) {
        fseek(bnk_file, (bnk_version <= 0x59 ? 9 : 5), SEEK_CUR);
        uint32_t num_vertices;
        assert(fread(&num_vertices, 4, 1, bnk_file) == 1);
        fseek(bnk_file, 16 * num_vertices, SEEK_CUR);
        uint32_t num_playlist_items;
        assert(fread(&num_playlist_items, 4, 1, bnk_file) == 1);
        dprintf("num vertices: %d, num_playlist items: %d, ftell: %ld\n", num_vertices, num_playlist_items, ftell(bnk_file));
        fseek(bnk_file, (bnk_version <= 0x59 ? 16 : 20) * num_playlist_items, SEEK_CUR);
    } else if (bnk_version <= 0x59) {
        getc(bnk_file);
    }
}

void skip_aux_params(FILE* bnk_file, uint32_t bnk_version)
{
    bool has_aux = (getc(bnk_file) >> 3) & 1;
    if (has_aux) fseek(bnk_file, 4 * sizeof(uint32_t), SEEK_CUR);
    if (bnk_version > 0x87) fseek(bnk_file, 4, SEEK_CUR);
}

void skip_state_chunk(FILE* bnk_file)
{
    uint8_t state_props = getc(bnk_file);
    fseek(bnk_file, 3 * state_props, SEEK_CUR);
    uint8_t state_groups = getc(bnk_file);
    for (uint8_t i = 0; i < state_groups; i++) {
        fseek(bnk_file, 5, SEEK_CUR);
        uint8_t states = getc(bnk_file);
        fseek(bnk_file, 8 * states, SEEK_CUR);
    }
}

void skip_rtpc(FILE* bnk_file, uint32_t bnk_version)
{
    uint16_t num_rtpc;
    assert(fread(&num_rtpc, 2, 1, bnk_file) == 1);
    for (int i = 0; i < num_rtpc; i++) {
        fseek(bnk_file, bnk_version <= 0x59 ? 13 : 12, SEEK_CUR);
        uint16_t point_count;
        assert(fread(&point_count, 2, 1, bnk_file) == 1);
        fseek(bnk_file, 12 * point_count, SEEK_CUR);
    }
}

uint32_t skip_base_params(FILE* bnk_file, uint32_t bnk_version, uint32_t* out_bus_id)
{
    skip_initial_fx_params(bnk_file, bnk_version);
    if (bnk_version > 0x88) {
        fseek(bnk_file, 1, SEEK_CUR);
        uint8_t num_fx = getc(bnk_file);
        fseek(bnk_file, 6 * num_fx, SEEK_CUR);
    }

    if (bnk_version > 0x59 && bnk_version <= 0x91) fseek(bnk_file, 1, SEEK_CUR);
    if (!out_bus_id) out_bus_id = &(uint32_t) {0};
    assert(fread(out_bus_id, 4, 1, bnk_file) == 1);
    dprintf("reading in parent id at position %ld\n", ftell(bnk_file));
    uint32_t parent_id;
    assert(fread(&parent_id, 4, 1, bnk_file) == 1);
    fseek(bnk_file, bnk_version <= 0x59 ? 2 : 1, SEEK_CUR);

    skip_initial_params(bnk_file);
    skip_positioning_params(bnk_file, bnk_version);
    skip_aux_params(bnk_file, bnk_version);

    fseek(bnk_file, 6, SEEK_CUR);

    uint8_t state_props = getc(bnk_file);
    fseek(bnk_file, 3 * state_props, SEEK_CUR);
    uint8_t state_groups = getc(bnk_file);
    for (uint8_t i = 0; i < state_groups; i++) {
        fseek(bnk_file, 5, SEEK_CUR);
        uint8_t states = getc(bnk_file);
        fseek(bnk_file, 8 * states, SEEK_CUR);
    }

    dprintf("skipping rtpc at position %ld\n", ftell(bnk_file));
    skip_rtpc(bnk_file, bnk_version);

    return parent_id;
}


#define free_simple_section(section) do { \
    free((section)->objects); \
} while (0)

void free_event_section(EventSection* section)
{
    for (uint32_t i = 0; i < section->length; i++) {
        free(section->objects[i].event_ids);
    }
    free(section->objects);
}

void free_random_container_section(RandomContainerSection* section)
{
    for (uint32_t i = 0; i < section->length; i++) {
        free(section->objects[i].sound_ids);
    }
    free(section->objects);
}

void free_music_container_section(MusicContainerSection* section)
{
    for (uint32_t i = 0; i < section->length; i++) {
        free(section->objects[i].music_track_ids);
    }
    free(section->objects);
}

int read_random_container_object(FILE* bnk_file, RandomContainerSection* random_containers, uint32_t bnk_version)
{
    struct random_container new_random_container_object;
    assert(fread(&new_random_container_object.self_id, 4, 1, bnk_file) == 1);
    dprintf("at the beginning: %ld\n", ftell(bnk_file));

    new_random_container_object.switch_container_id = skip_base_params(bnk_file, bnk_version, NULL);

    fseek(bnk_file, 24, SEEK_CUR);
    assert(fread(&new_random_container_object.sound_id_amount, 4, 1, bnk_file) == 1);
    dprintf("sound object id amount: %u\n", new_random_container_object.sound_id_amount);
    if (new_random_container_object.sound_id_amount > 100) {
        eprintf("Would have allocated %u bytes. That can't be right. (ERROR btw)\n", new_random_container_object.sound_id_amount * 4);
        return -1;
    }
    new_random_container_object.sound_ids = malloc(new_random_container_object.sound_id_amount * 4);
    assert(fread(new_random_container_object.sound_ids, 4, new_random_container_object.sound_id_amount, bnk_file) == new_random_container_object.sound_id_amount);

    add_object(random_containers, &new_random_container_object);

    return 0;
}

int read_sound_object(FILE* bnk_file, SoundSection* sounds, uint32_t bnk_version)
{
    struct sound new_sound_object;
    assert(fread(&new_sound_object.self_id, 4, 1, bnk_file) == 1);
    fseek(bnk_file, 4, SEEK_CUR);
    assert(fread(&new_sound_object.is_streamed, 1, 1, bnk_file) == 1);
    if (bnk_version == 0x58) fseek(bnk_file, 3, SEEK_CUR); // was 4 byte field with 3 bytes zero
    assert(fread(&new_sound_object.file_id, 4, 1, bnk_file) == 1);
    assert(fread(&new_sound_object.source_id, 4, 1, bnk_file) == 1);
    fseek(bnk_file, 8 - (bnk_version == 0x58), SEEK_CUR);
    assert(fread(&new_sound_object.sound_object_id, 4, 1, bnk_file) == 1);

    add_object(sounds, &new_sound_object);

    return 0;
}

int read_event_action_object(FILE* bnk_file, EventActionSection* event_actions)
{
    struct event_action new_event_action_object;
    assert(fread(&new_event_action_object.self_id, 4, 1, bnk_file) == 1);
    assert(fread(&new_event_action_object.scope, 1, 1, bnk_file) == 1);
    assert(fread(&new_event_action_object.type, 1, 1, bnk_file) == 1);
    if (new_event_action_object.type == 25 /* set switch */) {
        fseek(bnk_file, 5, SEEK_CUR);
        skip_initial_params(bnk_file);
        assert(fread(&new_event_action_object.switch_group_id, 4, 1, bnk_file) == 1);
    } else {
        assert(fread(&new_event_action_object.sound_object_id, 4, 1, bnk_file) == 1);
    }

    add_object(event_actions, &new_event_action_object);

    return 0;
}

int read_event_object(FILE* bnk_file, EventSection* events, uint32_t bnk_version)
{
    struct event new_event_object;
    assert(fread(&new_event_object.self_id, 4, 1, bnk_file) == 1);
    assert(fread(&new_event_object.event_amount, 1, 1, bnk_file) == 1);
    if (bnk_version == 0x58) fseek(bnk_file, 3, SEEK_CUR); // presumably padding bytes or 4 byte int which was later deemed unnecessarily high
    new_event_object.event_ids = malloc(new_event_object.event_amount * 4);
    assert(fread(new_event_object.event_ids, 4, new_event_object.event_amount, bnk_file) == new_event_object.event_amount);

    add_object(events, &new_event_object);

    return 0;
}

int read_music_container_object(FILE* bnk_file, MusicContainerSection* music_containers, uint32_t bnk_version)
{
    struct music_container new_music_container_object;
    assert(fread(&new_music_container_object.self_id, 4, 1, bnk_file) == 1);
    fseek(bnk_file, 1, SEEK_CUR);
    new_music_container_object.sound_object_id = skip_base_params(bnk_file, bnk_version, &new_music_container_object.music_switch_id);
    assert(fread(&new_music_container_object.music_track_id_amount, 4, 1, bnk_file) == 1);
    new_music_container_object.music_track_ids = malloc(new_music_container_object.music_track_id_amount * 4);
    assert(fread(new_music_container_object.music_track_ids, 4, new_music_container_object.music_track_id_amount, bnk_file) == new_music_container_object.music_track_id_amount);

    add_object(music_containers, &new_music_container_object);

    return 0;
}

int read_music_track_object(FILE* bnk_file, MusicTrackSection* music_tracks)
{
    struct music_track new_music_track_object;
    assert(fread(&new_music_track_object.self_id, 4, 1, bnk_file) == 1);
    fseek(bnk_file, 10, SEEK_CUR);
    assert(fread(&new_music_track_object.file_id, 4, 1, bnk_file) == 1);
    fseek(bnk_file, 64, SEEK_CUR);
    assert(fread(&new_music_track_object.music_container_id, 4, 1, bnk_file) == 1);

    add_object(music_tracks, &new_music_track_object);

    return 0;
}

int read_music_switch_object(FILE* bnk_file, MusicSwitchSection* music_switches)
{
    struct music_switch new_music_switch_object;
    assert(fread(&new_music_switch_object.self_id, 4, 1, bnk_file) == 1);
    fseek(bnk_file, 4, SEEK_CUR);
    assert(fread(&new_music_switch_object.some_id, 4, 1, bnk_file) == 1);

    add_object(music_switches, &new_music_switch_object);

    return 0;
}

void parse_bnk_file(char* path, SoundSection* sounds, EventActionSection* event_actions, EventSection* events, RandomContainerSection* random_containers, MusicContainerSection* music_segments, MusicTrackSection* music_tracks, MusicSwitchSection* music_switches, MusicContainerSection* music_playlists)
{
    FILE* bnk_file = fopen(path, "rb");
    if (!bnk_file) {
        eprintf("Error: Failed to open \"%s\".\n", path);
        exit(EXIT_FAILURE);
    }
    char magic[4];
    assert(fread(magic, 1, 4, bnk_file) == 4);
    if (memcmp(magic, "BKHD", 4) != 0) {
        eprintf("Error: Not a bnk file!\n");
        exit(EXIT_FAILURE);
    }
    fseek(bnk_file, 4, SEEK_CUR);
    uint32_t bnk_version;
    assert(fread(&bnk_version, 4, 1, bnk_file) == 1);

    uint32_t section_length = skip_to_section(bnk_file, "HIRC", true);
    if (!section_length) {
        eprintf("Error: Failed to skip to section \"HIRC\" in file \"%s\".\nMake sure to provide the correct file.\n", path);
        exit(EXIT_FAILURE);
    }
    uint32_t initial_position = ftell(bnk_file);
    uint32_t num_of_objects;
    assert(fread(&num_of_objects, 4, 1, bnk_file) == 1);
    uint32_t objects_read = 0;
    while ((uint32_t) ftell(bnk_file) < initial_position + section_length) {
        uint8_t type;
        uint32_t object_length;
        assert(fread(&type, 1, 1, bnk_file) == 1);
        assert(fread(&object_length, 4, 1, bnk_file) == 1);

        dprintf("Am here with an object of type %u\n", type);
        int object_start = ftell(bnk_file);
        switch (type)
        {
            case 2:
                read_sound_object(bnk_file, sounds, bnk_version);
                break;
            case 3:
                read_event_action_object(bnk_file, event_actions);
                break;
            case 4:
                read_event_object(bnk_file, events, bnk_version);
                break;
            case 5:
                read_random_container_object(bnk_file, random_containers, bnk_version);
                break;
            case 10:
                read_music_container_object(bnk_file, music_segments, bnk_version);
                break;
            case 11:
                read_music_track_object(bnk_file, music_tracks);
                break;
            case 12:
                read_music_switch_object(bnk_file, music_switches);
                break;
            case 13:
                read_music_container_object(bnk_file, music_playlists, bnk_version);
                break;
            default:
                dprintf("Skipping object, as it is irrelevant for me.\n");
                dprintf("gonna seek %u forward\n", object_length);
        }
        fseek(bnk_file, object_start + object_length, SEEK_SET);

        objects_read++;
    }
    dprintf("objects read: %u, num of objects: %u\n", objects_read, num_of_objects);
    assert(objects_read == num_of_objects);
    dprintf("Current offset: %ld\n", ftell(bnk_file));

    for (uint32_t i = 0; i < sounds->length; i++) {
        dprintf("sound object id: %u, source id: %u, file id: %u\n", sounds->objects[i].sound_object_id, sounds->objects[i].source_id, sounds->objects[i].file_id);
    }

    for (uint32_t i = 0; i < events->length; i++) {
        dprintf("Self id of all event objects: %u\n", events->objects[i].self_id);
    }
    dprintf("amount of event ids: %u\n", events->length);
    dprintf("amount of event actions: %u, amount of sounds: %u\n", event_actions->length, sounds->length);
    for (uint32_t i = 0; i < event_actions->length; i++) {
        dprintf("event action sound object ids: %u\n", event_actions->objects[i].sound_object_id);
    }

    fclose(bnk_file);
}


#define VERSION "1.8"
void print_help()
{
    printf("bnk-extract "VERSION" - a tool to extract bnk and wpk files, optionally sorting them into named groups.\n\n");
    printf("Syntax: ./bnk-extract --audio path/to/audio.[bnk|wpk] [--bin path/to/skinX.bin --events path/to/events.bnk] [-o path/to/output] [--wems-only] [--oggs-only]\n\n");
    printf("Options: \n");
    printf("  [-a|--audio] path\n    Specify the path to the audio bnk/wpk file that is to be extracted (mandatory).\n    Specifying this option without -e and -b will only extract files without grouping them by event name.\n\n");
    printf("  [-e|--events] path\n    Specify the path to the events bnk file that contains information about the events that trigger certain audio files.\n\n");
    printf("  [-b|--bin] path\n    Specify the path to the bin file that lists the clear names of all events.\n\n    Must specify both -e and -b options (or neither).\n\n");
    printf("  [-o|--output] path\n    Specify output path. Default is \"output\".\n\n");
    printf("  [--wems-only]\n    Extract wem files only.\n\n");
    printf("  [--oggs-only]\n    Extract ogg files only.\n    By default, both .wem and converted .ogg files will be extracted.\n\n");
    printf("  [--alternate-filenames]\n    Files will be named by event name + unique id instead of by their audio id.\n\n");
    printf("  [-v [-v ...]]\n    Increases verbosity level by one per \"-v\".\n");
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        eprintf("Missing arguments! (type --help for more info).\n");
        exit(EXIT_FAILURE);
    }
    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        print_help();
        exit(EXIT_FAILURE);
    }

    char* bin_path = NULL;
    char* audio_path = NULL;
    char* events_path = NULL;
    char* output_path = "output";
    bool wems_only = false;
    bool oggs_only = false;
    bool alternate_filenames = false;
    for (char** arg = &argv[1]; *arg; arg++) {
        if (strcmp(*arg, "-a") == 0 || strcmp(*arg, "--audio") == 0) {
            if (*(arg + 1)) {
                arg++;
                audio_path = *arg;
            }
        } else if (strcmp(*arg, "-e") == 0 || strcmp(*arg, "--events") == 0) {
            if (*(arg + 1)) {
                arg++;
                events_path = *arg;
            }
        } else if (strcmp(*arg, "-b") == 0 || strcmp(*arg, "--bin") == 0) {
            if (*(arg + 1)) {
                arg++;
                bin_path = *arg;
            }
        } else if (strcmp(*arg, "-o") == 0 || strcmp(*arg, "--output") == 0) {
            if (*(arg + 1)) {
                arg++;
                output_path = *arg;
            }
        } else if (strcmp(*arg, "--wem-only") == 0 || strcmp(*arg, "--wems-only") == 0) {
            wems_only = true;
        } else if (strcmp(*arg, "--ogg-only") == 0 || strcmp(*arg, "--oggs-only") == 0) {
            oggs_only = true;
        } else if (strcmp(*arg, "--alternate-filename") == 0 || strcmp(*arg, "--alternate-filenames") == 0) {
            alternate_filenames = true;
        } else if (strcmp(*arg, "-v") == 0) {
            VERBOSE++;
        }
    }
    if (!audio_path) {
        eprintf("Error: No audio file provided.\n");
        exit(EXIT_FAILURE);
    } else if (!events_path != !bin_path) { // one given, but not both
        eprintf("Error: Provide both events and bin file.\n");
        exit(EXIT_FAILURE);
    }

    StringHashes string_files;
    initialize_list(&string_files);
    if (!bin_path) {
        if (strlen(audio_path) >= 4 && memcmp(&audio_path[strlen(audio_path) - 4], ".bnk", 4) == 0)
            extract_bnk_file(audio_path, &string_files, output_path, wems_only, oggs_only, alternate_filenames);
        else
            extract_wpk_file(audio_path, &string_files, output_path, wems_only, oggs_only, alternate_filenames);
        free(string_files.objects);
        exit(EXIT_SUCCESS);
    }

    StringHashes* read_strings = parse_bin_file(bin_path);
    SoundSection sounds;
    EventActionSection event_actions;
    EventSection events;
    RandomContainerSection random_containers;
    MusicContainerSection music_segments;
    MusicTrackSection music_tracks;
    MusicSwitchSection music_switches;
    MusicContainerSection music_playlists;
    initialize_list(&sounds);
    initialize_list(&event_actions);
    initialize_list(&events);
    initialize_list(&random_containers);
    initialize_list(&music_segments);
    initialize_list(&music_tracks);
    initialize_list(&music_switches);
    initialize_list(&music_playlists);

    parse_bnk_file(events_path, &sounds, &event_actions, &events, &random_containers, &music_segments, &music_tracks, &music_switches, &music_playlists);
    sort_list(&event_actions, self_id);
    sort_list(&events, self_id);
    sort_list(&music_segments, self_id);
    sort_list(&music_switches, self_id);
    sort_list(&music_tracks, self_id);

    dprintf("amount: %u\n", read_strings->length);
    for (uint32_t i = 0; i < read_strings->length; i++) {
        uint32_t hash = read_strings->objects[i].hash;
        dprintf("hashes[%u]: %u, string: %s\n", i, read_strings->objects[i].hash, read_strings->objects[i].string);

        struct event* event = NULL;
        find_object_s(&events, event, self_id, hash);
        if (!event) continue;
        for (uint32_t j = 0; j < event->event_amount; j++) {
            struct event_action* event_action = NULL;
            find_object_s(&event_actions, event_action, self_id, event->event_ids[j]);
            if (event_action && event_action->type == 4 /* "play" */) {
                for (uint32_t k = 0; k < sounds.length; k++) {
                    if (sounds.objects[k].sound_object_id == event_action->sound_object_id || sounds.objects[k].self_id == event_action->sound_object_id) {
                        dprintf("Found one!\n");
                        v_printf(2, "Hash %u of string %s belongs to file \"%u.wem\".\n", hash, read_strings->objects[i].string, sounds.objects[k].file_id);
                        add_object(&string_files, (&(struct string_hash) {
                            .string = read_strings->objects[i].string,
                            .hash = sounds.objects[k].file_id,
                            .sound_index = k
                        }));
                    }
                }
                for (uint32_t k = 0; k < music_segments.length; k++) {
                    if (music_segments.objects[k].sound_object_id == event_action->sound_object_id) {
                        for (uint32_t l = 0; l < music_segments.objects[k].music_track_id_amount; l++) {
                            struct music_track* music_track = NULL;
                            find_object_s(&music_tracks, music_track, self_id, music_segments.objects[k].music_track_ids[l]);
                            if (!music_track) continue;
                            dprintf("Found one 1!\n");
                            v_printf(2, "Hash %u of string %s belongs to file \"%u.wem\".\n", hash, read_strings->objects[i].string, music_track->file_id);
                            add_object(&string_files, (&(struct string_hash) {
                                .string = read_strings->objects[i].string,
                                .hash = music_track->file_id,
                                .music_segment_id = music_segments.objects[k].self_id
                            }));
                        }
                    }
                }
                struct music_switch* music_switch = NULL;
                find_object_s(&music_switches, music_switch, self_id, event_action->sound_object_id);
                if (music_switch)
                for (uint32_t k = 0; k < music_playlists.length; k++) {
                    if (music_switch->some_id == music_playlists.objects[k].music_switch_id)
                    for (uint32_t m = 0; m < music_playlists.objects[k].music_track_id_amount; m++) {
                        struct music_container* music_segment = NULL;
                        find_object_s(&music_segments, music_segment, self_id, music_playlists.objects[k].music_track_ids[m]);
                        for (uint32_t n = 0; n < music_segment->music_track_id_amount; n++) {
                            struct music_track* music_track = NULL;
                            find_object_s(&music_tracks, music_track, self_id, music_segment->music_track_ids[n]);
                            if (!music_track) continue;
                            dprintf("Found one 3!\n");
                            v_printf(2, "Hash %u of string %s belongs to file \"%u.wem\".\n", hash, read_strings->objects[i].string, music_track->file_id);
                            add_object(&string_files, (&(struct string_hash) {
                                .string = read_strings->objects[i].string,
                                .hash = music_track->file_id,
                                .container_id = music_playlists.objects[k].self_id,
                                .music_segment_id = music_segment->self_id
                            }));
                        }
                    }
                }
                for (uint32_t k = 0; k < music_playlists.length; k++) {
                    if (music_playlists.objects[k].sound_object_id == event_action->sound_object_id) {
                        for (uint32_t l = 0; l < music_playlists.objects[k].music_track_id_amount; l++) {
                            struct music_container* music_segment = NULL;
                            find_object_s(&music_segments, music_segment, self_id, music_playlists.objects[k].music_track_ids[l]);
                            for (uint32_t m = 0; m < music_segment->music_track_id_amount; m++) {
                                struct music_track* music_track = NULL;
                                find_object_s(&music_tracks, music_track, self_id, music_segment->music_track_ids[m]);
                                if (!music_track) continue;
                                dprintf("Found one 2!\n");
                                v_printf(2, "Hash %u of string %s belongs to file \"%u.wem\".\n", hash, read_strings->objects[i].string, music_track->file_id);
                                add_object(&string_files, (&(struct string_hash) {
                                    .string = read_strings->objects[i].string,
                                    .hash = music_track->file_id,
                                    .music_segment_id = music_segment->self_id
                                }));
                            }
                        }
                    }
                }
                for (uint32_t k = 0; k < random_containers.length; k++) {
                    if (random_containers.objects[k].switch_container_id == event_action->sound_object_id) {
                        for (uint32_t l = 0; l < random_containers.objects[k].sound_id_amount; l++) {
                            for (uint32_t m = 0; m < sounds.length; m++) {
                                if (random_containers.objects[k].sound_ids[l] == sounds.objects[m].self_id || random_containers.objects[k].sound_ids[l] == sounds.objects[m].sound_object_id) {
                                    dprintf("sound id amount? %u\n", random_containers.objects[k].sound_id_amount);
                                    dprintf("Found one precisely here.\n");
                                    v_printf(2, "Hash %u of string %s belongs to file \"%u.wem\".\n", hash, read_strings->objects[i].string, sounds.objects[m].file_id);
                                    add_object(&string_files, (&(struct string_hash) {
                                        .string = read_strings->objects[i].string,
                                        .hash = sounds.objects[m].file_id,
                                        .container_id = random_containers.objects[k].self_id,
                                        .sound_index = m
                                    }));
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // separate "root" folder for music playlists that do not belong to anything else; maybe just dj sona?
    for (uint32_t i = 0; i < music_playlists.length; i++) {
        if (!music_playlists.objects[i].music_switch_id)
        for (uint32_t j = 0; j < music_playlists.objects[i].music_track_id_amount; j++) {
            struct music_container* music_segment = NULL;
            find_object_s(&music_segments, music_segment, self_id, music_playlists.objects[i].music_track_ids[j]);
            for (uint32_t k = 0; k < music_segment->music_track_id_amount; k++) {
                struct music_track* music_track = NULL;
                find_object_s(&music_tracks, music_track, self_id, music_segment->music_track_ids[k]);
                if (!music_track) continue;
                add_object(&string_files, (&(struct string_hash) {
                    .string = "Music playlists",
                    .hash = music_track->file_id,
                    .container_id = music_playlists.objects[i].self_id,
                    .music_segment_id = music_segment->self_id
                }));
            }
        }
    }

    free_simple_section(&sounds);
    free_simple_section(&event_actions);
    free_event_section(&events);
    free_random_container_section(&random_containers);
    free_music_container_section(&music_segments);
    free_simple_section(&music_tracks);
    free_simple_section(&music_switches);
    free_music_container_section(&music_playlists);

    sort_list(&string_files, hash);
    if (strlen(audio_path) >= 4 && memcmp(&audio_path[strlen(audio_path) - 4], ".bnk", 4) == 0)
        extract_bnk_file(audio_path, &string_files, output_path, wems_only, oggs_only, alternate_filenames);
    else
        extract_wpk_file(audio_path, &string_files, output_path, wems_only, oggs_only, alternate_filenames);

    free(string_files.objects);
    for (uint32_t i = 0; i < read_strings->length; i++) {
        free(read_strings->objects[i].string);
    }
    free(read_strings->objects);
    free(read_strings);
}
