#define __USE_MINGW_ANSI_STDIO
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <assert.h>
#include <string.h>

#include "defs.h"
#include "general_utils.h"
#include "bin.h"
#include "bnk.h"
#include "wpk.h"

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
    uint32_t switch_group_id;
};

struct actor_mixer {
    uint32_t self_id;
    uint32_t unk1_id;
    uint32_t unk2_id;
    uint32_t sound_object_id_amount;
    uint32_t* sound_object_ids;
};


typedef LIST(struct sound) SoundSection;
typedef LIST(struct event_action) EventActionSection;
typedef LIST(struct event) EventSection;
typedef LIST(struct random_container) RandomContainerSection;
typedef LIST(struct switch_container) SwitchContainerSection;
typedef LIST(struct actor_mixer) ActorMixerSection;


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

void free_actor_mixer_section(ActorMixerSection* section)
{
    for (uint32_t i = 0; i < section->length; i++) {
        free(section->objects[i].sound_object_ids);
    }
    free(section->objects);
}

int read_random_container_object(FILE* bnk_file, uint32_t object_length, RandomContainerSection* random_containers)
{
    uint32_t initial_position = ftell(bnk_file);
    struct random_container new_random_container_object;
    assert(fread(&new_random_container_object.self_id, 4, 1, bnk_file) == 1);
    uint16_t unk;
    assert(fread(&unk, 2, 1, bnk_file) == 1);
    int to_seek;
    dprintf("at the beginning: %ld\n", ftell(bnk_file));
    switch (unk)
    {
        case 0:
        case 1:
            to_seek = 5;
            break;
        case 769:
            to_seek = 27;
            break;
        case 1025:
            to_seek = 34;
            break;
        default:
            eprintf("Have to improve my parsing; unknown type encountered (%u).\n", unk);
            exit(EXIT_FAILURE);
    }
    fseek(bnk_file, to_seek, SEEK_CUR);
    dprintf("reading in switch container id at position %ld\n", ftell(bnk_file));
    assert(fread(&new_random_container_object.switch_container_id, 4, 1, bnk_file) == 1);
    fseek(bnk_file, 1, SEEK_CUR);
    uint8_t unk2;
    assert(fread(&unk2, 1, 1, bnk_file) == 1);
    uint8_t unk3_part[unk2 - 1];
    assert(fread(unk3_part, 1, unk2 - 1, bnk_file) == unk2 - 1u);
    int unk3 = getc(bnk_file);
    dprintf("offset here: %ld, unk2: %d, unk3: %x\n", ftell(bnk_file), unk2, unk3);
    fseek(bnk_file, 4 * unk2, SEEK_CUR);
    to_seek = 35;
    if (unk2 > 2 && unk3 == 0x3b && unk3_part[unk2 - 2] == 6)
        to_seek += unk3;
    if (getc(bnk_file)) {
        to_seek--;
        fseek(bnk_file, 1, SEEK_CUR);
    }
    else
        to_seek += getc(bnk_file) != 0;
    if (unk2 == 3 && unk3 == 7) {
        to_seek += 10;
    } else if (unk2 > 2 && unk3 != 7 && unk3_part[0] != 6)
        to_seek++;
    dprintf("ftell before the seek: %ld\n", ftell(bnk_file));
    dprintf("gonna seek %d\n", to_seek);
    fseek(bnk_file, to_seek, SEEK_CUR);
    assert(fread(&new_random_container_object.sound_id_amount, 4, 1, bnk_file) == 1);
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
    fseek(bnk_file, 28, SEEK_CUR);
    assert(fread(&new_switch_container_object.switch_group_id, 4, 1, bnk_file) == 1);

    fseek(bnk_file, initial_position + object_length, SEEK_SET);
    add_object(switch_containers, &new_switch_container_object);

    return 0;
}

int read_actor_mixer_object(FILE* bnk_file, ActorMixerSection* actor_mixers)
{
    dprintf("offset at the beginning: %ld\n", ftell(bnk_file));
    struct actor_mixer new_actor_mixer_object;
    assert(fread(&new_actor_mixer_object.self_id, 4, 1, bnk_file) == 1);
    fseek(bnk_file, 1, SEEK_CUR);
    int to_seek = 1;
    int unk = fgetc(bnk_file);
    if (unk == 2)
        to_seek += 15;
    else if (unk == 4)
        to_seek += 29;
    else if (unk != 0) {
        eprintf("Please consider this as an error. Type here was unknown and %u\n", unk);
        exit(EXIT_FAILURE);
    }
    fseek(bnk_file, to_seek, SEEK_CUR);
    assert(fread(&new_actor_mixer_object.unk1_id, 4, 1, bnk_file) == 1);
    assert(fread(&new_actor_mixer_object.unk2_id, 4, 1, bnk_file) == 1);
    fseek(bnk_file, 1, SEEK_CUR);
    to_seek = 6;
    int unk2 = fgetc(bnk_file);
    if (unk2 == 1)
        to_seek += 5;
    else if (unk2 == 2)
        to_seek += 11;
    else if (unk2 == 3)
        to_seek += 16;
    else if (unk2 != 0) {
        eprintf("Consider this an error. Encountered unknown type %u\n", unk2);
        exit(EXIT_FAILURE);
    }
    fseek(bnk_file, 6, SEEK_CUR);
    if (fgetc(bnk_file) == 3)
        to_seek++;
    fseek(bnk_file, to_seek, SEEK_CUR);
    assert(fread(&new_actor_mixer_object.sound_object_id_amount, 4, 1, bnk_file) == 1);
    dprintf("after reading in sound object id amount, i'm at %ld\n", ftell(bnk_file));
    dprintf("amount to allocate: %u\n", new_actor_mixer_object.sound_object_id_amount);
    if (new_actor_mixer_object.sound_object_id_amount > 100 || new_actor_mixer_object.sound_object_id_amount == 0) {
        eprintf("this is an error. consider this.\n");
        exit(EXIT_FAILURE);
    }
    new_actor_mixer_object.sound_object_ids = malloc(new_actor_mixer_object.sound_object_id_amount * 4);
    assert(fread(new_actor_mixer_object.sound_object_ids, 4, new_actor_mixer_object.sound_object_id_amount, bnk_file) == new_actor_mixer_object.sound_object_id_amount);

    add_object(actor_mixers, &new_actor_mixer_object);
    dprintf("offset here: %ld\n", ftell(bnk_file));

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


void parse_bnk_file(char* path, SoundSection* sounds, EventActionSection* event_actions, EventSection* events, RandomContainerSection* random_containers, SwitchContainerSection* switch_containers, ActorMixerSection* actor_mixers)
{
    FILE* bnk_file = fopen(path, "rb");
    if (!bnk_file) {
        eprintf("Error: Failed to open \"%s\".\n", path);
        exit(EXIT_FAILURE);
    }

    uint32_t section_length = skip_to_section(bnk_file, "HIRC", false);
    uint32_t initial_position = ftell(bnk_file);
    uint32_t num_of_objects;
    assert(fread(&num_of_objects, 4, 1, bnk_file) == 1);
    uint32_t objects_read = 0;
    while ((uint32_t) ftell(bnk_file) < initial_position + section_length) {
        uint8_t type;
        uint32_t object_length;
        assert(fread(&type, 1, 1, bnk_file) == 1);
        assert(fread(&object_length, 4, 1, bnk_file) == 1);
        if (type != 2 && type != 3 && type != 4 && type != 5 && type != 6 && type != 7) {
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
                break;
            case 7:
                read_actor_mixer_object(bnk_file, actor_mixers);
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


int main(int argc, char* argv[])
{
    if (argc < 4) {
        eprintf("Use bnk-extract to extract any .wpk or .bnk file to an output folder by providing paths to its location and the corresponding .bin and .bnk files.\n");
        eprintf("Usage: %s path/to/binfile.bin  path/to/bnkfile.bnk  path/to/file_to_extract[.wpk|.bnk] [-o outputpath] [--delete-wems] [-v ...]\n\nExample: %s Annie/skin0.bin Annie/annie_base_vo_events.bnk Annie/annie_base_vo_audio.wpk -o Annie_files\n", argv[0], argv[0]);
        exit(EXIT_FAILURE);
    }
    StringHashes* read_strings = parse_bin_file(argv[1]);
    char* bnk_path = argv[2];
    char* extract_path = argv[3];

    char* output_path = "output";
    bool delete_wems = false;
    for (int i = 4; argv[i]; i++) {
        if (strcmp(argv[i], "-o") == 0) {
            if (argv[i + 1]) {
                output_path = argv[i + 1];
                i++;
            } else {
                eprintf("WARNING: Ignoring option \"-o\" with missing value\n");
            }
        } else if (strcmp(argv[i], "--delete-wems") == 0 || strcmp(argv[i], "-d") == 0) {
            delete_wems = true;
        } else if (strcmp(argv[i], "-v") == 0) {
            VERBOSE++;
        }
    }
    SoundSection sounds = {0};
    EventActionSection event_actions = {0};
    EventSection events = {0};
    RandomContainerSection random_containers = {0};
    SwitchContainerSection switch_containers = {0};
    ActorMixerSection actor_mixers = {0};


    parse_bnk_file(bnk_path, &sounds, &event_actions, &events, &random_containers, &switch_containers, &actor_mixers);

    for (uint32_t i = 0; i < read_strings->length; i++) {
        dprintf("hashes[%u]: %u\n", i, read_strings->objects[i].hash);
        dprintf("this is the hash for string \"%s\"\n", read_strings->objects[i].string);
    }

    StringHashes string_files = {0};

    dprintf("amount: %u\n", read_strings->length);
    for (uint32_t i = 0; i < read_strings->length; i++) {
        uint32_t hash = read_strings->objects[i].hash;
        dprintf("hashes[%u]: %u, string: %s\n", i, read_strings->objects[i].hash, read_strings->objects[i].string);
        for (uint32_t j = 0; j < events.length; j++) {
            if (events.objects[j].self_id == hash) {
                for (int k = 0; k < events.objects[j].event_amount; k++) {
                    uint32_t event_id = events.objects[j].event_ids[k];
                    for (uint32_t l = 0; l < event_actions.length; l++) {
                        if (event_actions.objects[l].self_id == event_id) {
                            if (event_actions.objects[l].type == 4) {
                                uint32_t sound_object_id = event_actions.objects[l].sound_object_id;
                                dprintf("event type: %d\n", event_actions.objects[l].type);
                                for (uint32_t m = 0; m < sounds.length; m++) {
                                    if (sounds.objects[m].sound_object_id == sound_object_id || sounds.objects[m].self_id == sound_object_id) {
                                        dprintf("Found one!\n");
                                        vprintf(2, "Hash %u of string %s belongs to file \"%u.wem\".\n", hash, read_strings->objects[i].string, sounds.objects[m].file_id);
                                        add_object(&string_files, (&(struct string_hash) {read_strings->objects[i].string, sounds.objects[m].file_id}));
                                        dprintf("amount: %u\n", string_files.length);
                                    }
                                }
                                for (uint32_t m = 0; m < random_containers.length; m++) {
                                    if (random_containers.objects[m].switch_container_id == sound_object_id) {
                                        // printf("sound object id that was found: %u\n", sound_object_id);
                                        for (uint32_t n = 0; n < random_containers.objects[m].sound_id_amount; n++) {
                                            for (uint32_t o = 0; o < sounds.length; o++) {
                                                if (random_containers.objects[m].sound_ids[n] == sounds.objects[o].self_id || random_containers.objects[m].sound_ids[n] == sounds.objects[o].sound_object_id) {
                                                    dprintf("Found one precisely here.\n");
                                                    vprintf(2, "Hash %u of string %s belongs to file \"%u.wem\".\n", hash, read_strings->objects[i].string, sounds.objects[o].file_id);
                                                    add_object(&string_files, (&(struct string_hash) {read_strings->objects[i].string, sounds.objects[o].file_id}));
                                                    dprintf("amount: %u\n", string_files.length);
                                                }
                                            }
                                        }
                                    }
                                }
                                for (uint32_t m = 0; m < actor_mixers.length; m++) {
                                    if (actor_mixers.objects[m].unk2_id != 0 && actor_mixers.objects[m].sound_object_id_amount >= 1 && actor_mixers.objects[m].sound_object_id_amount < 6) { // 6 is an arbitrary number here; there are actor mixer objects that contain too many references, so I try to filter here
                                        for (uint32_t n = 0; n < actor_mixers.objects[m].sound_object_id_amount; n++) {
                                            if (actor_mixers.objects[m].sound_object_ids[n] == sound_object_id) {
                                                for (uint32_t o = 0; o < actor_mixers.objects[m].sound_object_id_amount; o++) {
                                                    // if (o != n) {
                                                        for (uint32_t p = 0; p < sounds.length; p++) {
                                                            // printf("sound object id that was searched: %d\n", sound_object_id);
                                                            if (sounds.objects[p].sound_object_id == actor_mixers.objects[m].sound_object_ids[o] || (actor_mixers.objects[m].sound_object_id_amount == 1 && sounds.objects[p].self_id == actor_mixers.objects[m].sound_object_ids[o])) {
                                                                dprintf("got one here!! YES, belongs to %u\n", sounds.objects[p].file_id);
                                                                vprintf(2, "Hash %u of string %s belongs to file \"%u.wem\".\n", hash, read_strings->objects[i].string, sounds.objects[p].file_id);
                                                                add_object(&string_files, (&(struct string_hash) {read_strings->objects[i].string, sounds.objects[p].file_id}));
                                                                dprintf("amount: %u\n", string_files.length);
                                                            }
                                                        }
                                                    // }
                                                }
                                            }
                                        }
                                    }
                                }
                            } else if (event_actions.objects[l].type == 25) {
                                dprintf("event action sounds object id here: %u\n", event_actions.objects[l].sound_object_id);
                                for (uint32_t m = 0; m < switch_containers.length; m++) {
                                    if (event_actions.objects[l].switch_group_id == switch_containers.objects[m].switch_group_id) {
                                        for (uint32_t n = 0; n < random_containers.length; n++) {
                                            if (random_containers.objects[n].switch_container_id == switch_containers.objects[m].self_id) {
                                                for (uint32_t o = 0; o < random_containers.objects[n].sound_id_amount; o++) {
                                                    for (uint32_t p = 0; p < sounds.length; p++) {
                                                        if (random_containers.objects[n].sound_ids[o] == sounds.objects[p].self_id) {
                                                            dprintf("Found one down here!\n");
                                                            vprintf(2, "Hash %u of string %s belongs to file \"%u.wem\".\n", hash, read_strings->objects[i].string, sounds.objects[p].file_id);
                                                            add_object(&string_files, (&(struct string_hash) {read_strings->objects[i].string, sounds.objects[p].file_id}));
                                                            dprintf("amount: %u\n", string_files.length);
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
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
    free_actor_mixer_section(&actor_mixers);

    if (strlen(extract_path) >= 4 && memcmp(&extract_path[strlen(extract_path) - 4], ".bnk", 4) == 0)
        extract_bnk_file(extract_path, &string_files, output_path, delete_wems);
    else
        extract_wpk_file(extract_path, &string_files, output_path, delete_wems);

    free(string_files.objects);
    for (uint32_t i = 0; i < read_strings->length; i++) {
        free(read_strings->objects[i].string);
    }
    free(read_strings->objects);
    free(read_strings);
}
