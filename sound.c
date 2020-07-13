#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <assert.h>
#include <string.h>

#include "defs.h"
#include "bin.h"
#include "bnk.h"
#include "wpk.h"

int VERBOSE = 0;

struct sound {
    uint32_t self_id;
    uint32_t file_id;
    uint32_t source_id;
    uint32_t sound_object_id;
    uint8_t is_streamed;
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

// this one is pure guessing
struct random_container {
    uint32_t self_id;
    uint32_t switch_container_id;
    uint32_t sound_id_amount;
    uint32_t* sound_ids;
};

struct switch_container {
    uint32_t self_id;
};

typedef LIST(struct sound) SoundSection;
typedef LIST(struct event_action) EventActionSection;
typedef LIST(struct event) EventSection;
typedef LIST(struct random_container) RandomContainerSection;
typedef LIST(struct switch_container) SwitchContainerSection;


void free_sound_section(SoundSection* section)
{
    free(section->objects);
}

void free_event_action_section(EventActionSection* section)
{
    free(section->objects);
}

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

void free_switch_container_section(SwitchContainerSection* section)
{
    free(section->objects);
}

int read_random_container_object(FILE* bnk_file, uint32_t object_length, RandomContainerSection* random_containers)
{
    uint32_t initial_position = ftell(bnk_file);
    struct random_container new_random_container_object;
    assert(fread(&new_random_container_object.self_id, 4, 1, bnk_file) == 1);
    dprintf("at the beginning: %ld\n", ftell(bnk_file));
    fseek(bnk_file, 1, SEEK_CUR);
    uint8_t unk = getc(bnk_file);
    fseek(bnk_file, 5 + (unk != 0) + (unk * 7), SEEK_CUR);
    dprintf("reading in switch container id at position %ld\n", ftell(bnk_file));
    assert(fread(&new_random_container_object.switch_container_id, 4, 1, bnk_file) == 1);
    fseek(bnk_file, 1, SEEK_CUR);
    uint8_t unk2 = getc(bnk_file);
    int to_seek = 25;
    if (unk2 != 0) {
        dprintf("offset here: %ld, unk2: %d\n", ftell(bnk_file), unk2);
        fseek(bnk_file, 5 * unk2, SEEK_CUR);
    }
    uint8_t unk3 = getc(bnk_file);
    dprintf("unk3: %d\n", unk3);
    fseek(bnk_file, 9 * unk3, SEEK_CUR);
    fseek(bnk_file, 9 + (getc(bnk_file) > 1), SEEK_CUR);
    dprintf("where am i? %ld\n", ftello(bnk_file));
    uint8_t unk4 = getc(bnk_file);
    dprintf("unk4: %d\n", unk4);
    if (unk4 && unk2) {
        fseek(bnk_file, 13, SEEK_CUR);
        uint8_t unk5 = getc(bnk_file);
        dprintf("unk5: %d\n", unk5);
        to_seek += (4 + unk4 * 8) * unk5 + (unk4 - 1) * 26;
        // this isn't perfect, it still fails for rammus skin 16 and gangplank skin 8+. TODO
    }
    dprintf("ftell before the seek: %ld\n", ftell(bnk_file));
    dprintf("gonna seek %d\n", to_seek);
    fseek(bnk_file, to_seek, SEEK_CUR);
    assert(fread(&new_random_container_object.sound_id_amount, 4, 1, bnk_file) == 1);
    dprintf("sound object id amount: %u\n", new_random_container_object.sound_id_amount);
    if (new_random_container_object.sound_id_amount > 100) {
        eprintf("Would have allocated %u bytes. That can't be right. (ERROR btw)\n", new_random_container_object.sound_id_amount * 4);
        exit(EXIT_FAILURE);
    }
    new_random_container_object.sound_ids = malloc(new_random_container_object.sound_id_amount * 4);
    assert(fread(new_random_container_object.sound_ids, 4, new_random_container_object.sound_id_amount, bnk_file) == new_random_container_object.sound_id_amount);

    fseek(bnk_file, initial_position + object_length, SEEK_SET);
    add_object(random_containers, &new_random_container_object);

    return 0;
}

int read_switch_container_object(FILE* bnk_file, uint32_t object_length, SwitchContainerSection* switch_containers)
{
    uint32_t initial_position = ftell(bnk_file);
    struct switch_container new_switch_container_object;
    assert(fread(&new_switch_container_object.self_id, 4, 1, bnk_file) == 1);
    fseek(bnk_file, initial_position + object_length, SEEK_SET);
    add_object(switch_containers, &new_switch_container_object);

    return 0;
}

int read_sound_object(FILE* bnk_file, uint32_t object_length, SoundSection* sounds)
{
    uint32_t initial_position = ftell(bnk_file);
    struct sound new_sound_object;
    assert(fread(&new_sound_object.self_id, 4, 1, bnk_file) == 1);
    fseek(bnk_file, 4, SEEK_CUR);
    assert(fread(&new_sound_object.is_streamed, 1, 1, bnk_file) == 1);
    assert(fread(&new_sound_object.file_id, 4, 1, bnk_file) == 1);
    assert(fread(&new_sound_object.source_id, 4, 1, bnk_file) == 1);
    fseek(bnk_file, 8, SEEK_CUR);
    assert(fread(&new_sound_object.sound_object_id, 4, 1, bnk_file) == 1);

    fseek(bnk_file, initial_position + object_length, SEEK_SET);
    add_object(sounds, &new_sound_object);

    return 0;
}

int read_event_action_object(FILE* bnk_file, uint32_t object_length, EventActionSection* event_actions)
{
    uint32_t initial_position = ftell(bnk_file);
    struct event_action new_event_action_object;
    assert(fread(&new_event_action_object.self_id, 4, 1, bnk_file) == 1);
    assert(fread(&new_event_action_object.scope, 1, 1, bnk_file) == 1);
    assert(fread(&new_event_action_object.type, 1, 1, bnk_file) == 1);
    if (new_event_action_object.type == 25) {
        fseek(bnk_file, 7, SEEK_CUR);
        assert(fread(&new_event_action_object.switch_group_id, 4, 1, bnk_file) == 1);
    } else {
        assert(fread(&new_event_action_object.sound_object_id, 4, 1, bnk_file) == 1);
    }

    fseek(bnk_file, initial_position + object_length, SEEK_SET);
    add_object(event_actions, &new_event_action_object);

    return 0;
}

int read_event_object(FILE* bnk_file, EventSection* events)
{
    struct event new_event_object;
    assert(fread(&new_event_object.self_id, 4, 1, bnk_file) == 1);
    assert(fread(&new_event_object.event_amount, 1, 1, bnk_file) == 1);
    new_event_object.event_ids = malloc(new_event_object.event_amount * 4);
    assert(fread(new_event_object.event_ids, 4, new_event_object.event_amount, bnk_file) == new_event_object.event_amount);

    add_object(events, &new_event_object);

    return 0;
}


void parse_bnk_file(char* path, SoundSection* sounds, EventActionSection* event_actions, EventSection* events, RandomContainerSection* random_containers, SwitchContainerSection* switch_containers)
{
    FILE* bnk_file = fopen(path, "rb");
    if (!bnk_file) {
        eprintf("Error: Failed to open \"%s\".\n", path);
        exit(EXIT_FAILURE);
    }

    uint32_t section_length = skip_to_section(bnk_file, "HIRC", false);
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
        if (type != 2 && type != 3 && type != 4 && type != 5 && type != 6) {
            dprintf("Skipping object with type %u, as it is irrelevant for me.\n", type);
            dprintf("gonna seek %u forward\n", object_length);
            fseek(bnk_file, object_length, SEEK_CUR);
            objects_read++;
            continue;
        }

        dprintf("Am here with an object of type %u\n", type);
        switch (type)
        {
            case 2:
                read_sound_object(bnk_file, object_length, sounds);
                break;
            case 3:
                read_event_action_object(bnk_file, object_length, event_actions);
                break;
            case 4:
                read_event_object(bnk_file, events);
                break;
            case 5:
                read_random_container_object(bnk_file, object_length, random_containers);
                break;
            case 6:
                read_switch_container_object(bnk_file, object_length, switch_containers);
        }

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


void print_help()
{
    printf("bnk-extract - a tool to extract bnk and wpk files, optionally sorting them into named groups.\n\n");
    printf("Options: \n");
    printf("  [-a|--audio] path\n    Specify the path to the audio bnk/wpk file that is to be extracted (mandatory).\n    Specifying this option without -e and -b will only extract files without grouping them by event name.\n\n");
    printf("  [-e|--events] path\n    Specify the path to the events bnk file that contains information about the events that trigger certain audio files.\n\n");
    printf("  [-b|--bin] path\n    Specify the path to the bin file that lists the clear names of all events.\n\n    Must specify both -e and -b options (or neither).\n\n");
    printf("  [-o|--output] path\n    Specify output path. Default is \"output\".\n\n");
    printf("  [--wems-only]\n    Extract wem files only.\n\n");
    printf("  [-oggs-only]\n    Extract ogg files only.\n    By default, both .wem and converted .ogg files will be extracted.\n\n");
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
            extract_bnk_file(audio_path, &string_files, output_path, wems_only, oggs_only);
        else
            extract_wpk_file(audio_path, &string_files, output_path, wems_only, oggs_only);
        free(string_files.objects);
        exit(EXIT_SUCCESS);
    }

    StringHashes* read_strings = parse_bin_file(bin_path);
    SoundSection sounds;
    EventActionSection event_actions;
    EventSection events;
    RandomContainerSection random_containers;
    SwitchContainerSection switch_containers;
    initialize_list(&sounds);
    initialize_list(&event_actions);
    initialize_list(&events);
    initialize_list(&random_containers);
    initialize_list(&switch_containers);

    parse_bnk_file(events_path, &sounds, &event_actions, &events, &random_containers, &switch_containers);
    sort_list(&event_actions, self_id);
    sort_list(&events, self_id);
    sort_list(&switch_containers, self_id);

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
                        add_object(&string_files, (&(struct string_hash) {read_strings->objects[i].string, sounds.objects[k].file_id, 0}));
                        dprintf("amount: %u\n", string_files.length);
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
                                    struct switch_container* switch_container = NULL;
                                    find_object_s(&switch_containers, switch_container, self_id, random_containers.objects[k].switch_container_id);
                                    add_object(&string_files, (&(struct string_hash) {read_strings->objects[i].string, sounds.objects[m].file_id, switch_container ? random_containers.objects[k].self_id : 0}));
                                    dprintf("amount: %u\n", string_files.length);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    free_sound_section(&sounds);
    free_event_action_section(&event_actions);
    free_event_section(&events);
    free_random_container_section(&random_containers);
    free_switch_container_section(&switch_containers);

    if (strlen(audio_path) >= 4 && memcmp(&audio_path[strlen(audio_path) - 4], ".bnk", 4) == 0)
        extract_bnk_file(audio_path, &string_files, output_path, wems_only, oggs_only);
    else
        extract_wpk_file(audio_path, &string_files, output_path, wems_only, oggs_only);

    free(string_files.objects);
    for (uint32_t i = 0; i < read_strings->length; i++) {
        free(read_strings->objects[i].string);
    }
    free(read_strings->objects);
    free(read_strings);
}
