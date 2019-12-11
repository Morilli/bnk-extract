#define __USE_MINGW_ANSI_STDIO
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <assert.h>
#include <string.h>
#include <byteswap.h>

#include "defs.h"
#include "general_utils.h"
#include "bin.h"
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

struct random_container_section {
    uint32_t length;
    uint32_t allocated_length;
    struct random_container* objects;
};

struct switch_container_section {
    uint32_t length;
    uint32_t allocated_length;
    struct switch_container* objects;
};

struct actor_mixer_section {
    uint32_t length;
    uint32_t allocated_length;
    struct actor_mixer* objects;
};

struct sound_section {
    uint32_t length;
    uint32_t allocated_length;
    struct sound* objects;
};

struct event_action_section {
    uint32_t length;
    uint32_t allocated_length;
    struct event_action* objects;
};

struct event_section {
    uint32_t length;
    uint32_t allocated_length;
    struct event* objects;
};

/*
#define section(A) struct A##_section { \
    uint32_t length; \
    uint32_t allocated_length; \
    struct A* objects; \
}
section(test);
*/


#define add_object(list, object) { \
    if (list->allocated_length == 0) { \
        list->objects = malloc(16 * sizeof(typeof(*object))); \
        list->length = 0; \
        list->allocated_length = 16; \
    } else if (list->length == list->allocated_length) { \
        list->objects = realloc(list->objects, (list->allocated_length + (list->allocated_length >> 1)) * sizeof(typeof(*object))); \
        list->allocated_length += list->allocated_length >> 1; \
    } \
 \
    memcpy(&list->objects[list->length], object, sizeof(typeof(*object))); \
    list->length++; \
}

int read_random_container_object(FILE* bnk_file, uint32_t object_length, struct random_container_section* random_containers)
{
    uint32_t initial_position = ftell(bnk_file);
    struct random_container new_random_container_object;
    fread(&new_random_container_object.self_id, 4, 1, bnk_file);
    uint16_t unk;
    fread(&unk, 2, 1, bnk_file);
    int to_seek;
    printf("at the beginning: %ld\n", ftell(bnk_file));
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
            printf("Have to improve my parsing; unknown type encountered (%u).\n", unk);
            exit(EXIT_FAILURE);
    }
    fseek(bnk_file, to_seek, SEEK_CUR);
    printf("reading in switch container id at position %ld\n", ftell(bnk_file));
    fread(&new_random_container_object.switch_container_id, 4, 1, bnk_file);
    fseek(bnk_file, 1, SEEK_CUR);
    uint8_t unk2;
    fread(&unk2, 1, 1, bnk_file);
    printf("offset here: %ld, unk2: %d\n", ftell(bnk_file), unk2);
    switch (unk2)
    {
        case 0:
            to_seek = 37;
            break;
        case 1:
            fseek(bnk_file, 6, SEEK_CUR);
            to_seek = 35;
            if (getc(bnk_file) == 3)
                to_seek++;
            break;
        case 2:
            fseek(bnk_file, 10, SEEK_CUR);
            to_seek = 35;
            if (getc(bnk_file) == 1)
                to_seek += 10;
            else if (getc(bnk_file) == 3)
                to_seek++;
            break;
        case 3:
            fseek(bnk_file, 16, SEEK_CUR);
            to_seek = 35;
            int unk3 = getc(bnk_file);
            if (unk3 == 2)
                to_seek += 59;
            else if (unk3 == 3)
                to_seek++;
            break;
        case 4:
            to_seek = 116;
            break;
        default:
            printf("Unknown value encountered (%u). Consider this an error as I'll kill the program now for attention.\n", unk2);
            exit(EXIT_FAILURE);
    }
    printf("gonna seek %d\n", to_seek);
    fseek(bnk_file, to_seek, SEEK_CUR);
    fread(&new_random_container_object.sound_id_amount, 4, 1, bnk_file);
    printf("going to allocate %u\n", new_random_container_object.sound_id_amount);
    printf("where the fuck am i? %ld\n", ftell(bnk_file));
    if (new_random_container_object.sound_id_amount > 100) {
        printf("holy. not allocating that much. (ERROR btw)\n");
        exit(EXIT_FAILURE);
    }
    new_random_container_object.sound_ids = malloc(new_random_container_object.sound_id_amount * 4);
    fread(new_random_container_object.sound_ids, 4, new_random_container_object.sound_id_amount, bnk_file);

    fseek(bnk_file, initial_position + object_length, SEEK_SET);
    add_object(random_containers, &new_random_container_object);

    return 0;
}

int read_switch_container_object(FILE* bnk_file, uint32_t object_length, struct switch_container_section* switch_containers)
{
    uint32_t initial_position = ftell(bnk_file);
    struct switch_container new_switch_container_object;
    fread(&new_switch_container_object.self_id, 4, 1, bnk_file);
    fseek(bnk_file, 28, SEEK_CUR);
    fread(&new_switch_container_object.switch_group_id, 4, 1, bnk_file);

    fseek(bnk_file, initial_position + object_length, SEEK_SET);
    add_object(switch_containers, &new_switch_container_object);

    return 0;
}

int read_actor_mixer_object(FILE* bnk_file, struct actor_mixer_section* actor_mixers)
{
    printf("offset at the beginning: %ld\n", ftell(bnk_file));
    struct actor_mixer new_actor_mixer_object;
    fread(&new_actor_mixer_object.self_id, 4, 1, bnk_file);
    fseek(bnk_file, 1, SEEK_CUR);
    int to_seek = 1;
    int unk = fgetc(bnk_file);
    if (unk == 2)
        to_seek += 15;
    else if (unk != 0) {
        printf("Please consider this as an error. Type here was unknown and %u\n", unk);
        exit(EXIT_FAILURE);
    }
    fseek(bnk_file, to_seek, SEEK_CUR);
    fread(&new_actor_mixer_object.unk1_id, 4, 1, bnk_file);
    fread(&new_actor_mixer_object.unk2_id, 4, 1, bnk_file);
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
        printf("Consider this an error. Encountered unknown type %u\n", unk2);
        exit(EXIT_FAILURE);
    }
    fseek(bnk_file, 6, SEEK_CUR);
    if (fgetc(bnk_file) == 3)
        to_seek++;
    fseek(bnk_file, to_seek, SEEK_CUR);
    fread(&new_actor_mixer_object.sound_object_id_amount, 4, 1, bnk_file);
    printf("after reading in sound object id amount, i'm at %ld\n", ftell(bnk_file));
    printf("amount to allocate: %u\n", new_actor_mixer_object.sound_object_id_amount);
    if (new_actor_mixer_object.sound_object_id_amount > 100 || new_actor_mixer_object.sound_object_id_amount == 0) {
        printf("this is an error. consider this.\n");
        exit(EXIT_FAILURE);
    }
    new_actor_mixer_object.sound_object_ids = malloc(new_actor_mixer_object.sound_object_id_amount * 4);
    fread(new_actor_mixer_object.sound_object_ids, 4, new_actor_mixer_object.sound_object_id_amount, bnk_file);

    add_object(actor_mixers, &new_actor_mixer_object);
    printf("offset here: %ld\n", ftell(bnk_file));

    return 0;
}

int read_sound_object(FILE* bnk_file, uint32_t object_length, struct sound_section* sounds)
{
    uint32_t initial_position = ftell(bnk_file);
    struct sound new_sound_object;
    fread(&new_sound_object.self_id, 4, 1, bnk_file);
    fseek(bnk_file, 4, SEEK_CUR);
    fread(&new_sound_object.is_streamed, 1, 1, bnk_file);
    fread(&new_sound_object.file_id, 4, 1, bnk_file);
    fread(&new_sound_object.source_id, 4, 1, bnk_file);
    fseek(bnk_file, 8, SEEK_CUR);
    fread(&new_sound_object.sound_object_id, 4, 1, bnk_file);

    fseek(bnk_file, initial_position + object_length, SEEK_SET);
    add_object(sounds, &new_sound_object);

    return 0;
}

int read_event_action_object(FILE* bnk_file, uint32_t object_length, struct event_action_section* event_actions)
{
    uint32_t initial_position = ftell(bnk_file);
    struct event_action new_event_action_object;
    fread(&new_event_action_object.self_id, 4, 1, bnk_file);
    fread(&new_event_action_object.scope, 1, 1, bnk_file);
    fread(&new_event_action_object.type, 1, 1, bnk_file);
    // printf("scope: %u\n", new_event_action_object.scope);
    // assert(new_event_action_object.scope == 3);
    // printf("type: %u\n", new_event_action_object.type);
    // assert(new_event_action_object.type == 4);
    if (new_event_action_object.type == 25) {
        fseek(bnk_file, 7, SEEK_CUR);
        fread(&new_event_action_object.switch_group_id, 4, 1, bnk_file);
    } else {
        fread(&new_event_action_object.sound_object_id, 4, 1, bnk_file);
    }

    fseek(bnk_file, initial_position + object_length, SEEK_SET);
    add_object(event_actions, &new_event_action_object);

    return 0;
}

int read_event_object(FILE* bnk_file, struct event_section* events)
{
    struct event new_event_object;
    fread(&new_event_object.self_id, 4, 1, bnk_file);
    fread(&new_event_object.event_amount, 1, 1, bnk_file);
    new_event_object.event_ids = malloc(new_event_object.event_amount * 4);
    fread(new_event_object.event_ids, 4, new_event_object.event_amount, bnk_file);

    add_object(events, &new_event_object);

    return 0;
}


int parse_bnk_file(char* path, struct sound_section* sounds, struct event_action_section* event_actions, struct event_section* events, struct random_container_section* random_containers, struct switch_container_section* switch_containers, struct actor_mixer_section* actor_mixers)
{
    FILE* bnk_file = fopen(path, "rb");
    if (!bnk_file) {
        fprintf(stderr, "Error: Failed to open \"%s\".\n", path);
        exit(EXIT_FAILURE);
    }
    uint8_t header[4] = {0};
    uint32_t length;
    read_header:
    printf("where are we in the file? %ld\n", ftell(bnk_file));
    if (fread(header, 1, 4, bnk_file) != 4 || fread(&length, 4, 1, bnk_file) != 1)
        return -1;
    if (memcmp(header, "BKHD", 4) == 0) {
        uint32_t version;
        fread(&version, 4, 1, bnk_file);
        printf("Version of sound bank: %u\n", version);
        fseek(bnk_file, length - 4, SEEK_CUR);
        goto read_header;
    } else if (memcmp(header, "HIRC", 4)) {
        fseek(bnk_file, length, SEEK_CUR);
        goto read_header;
    }

    printf("got here\n");
    uint32_t initial_position = ftell(bnk_file);
    uint32_t num_of_objects;
    fread(&num_of_objects, 4, 1, bnk_file);
    uint32_t objects_read = 0;
    while ((uint32_t) ftell(bnk_file) < initial_position + length) {
        uint8_t type;
        uint32_t object_length;
        fread(&type, 1, 1, bnk_file);
        fread(&object_length, 4, 1, bnk_file);
        if (type != 2 && type != 3 && type != 4 && type != 5 && type != 6 && type != 7) {
            printf("Skipping object with type %u, as it is irrelevant for me.\n", type);
            printf("gonna seek %u forward\n", object_length);
            fseek(bnk_file, object_length, SEEK_CUR);
            objects_read++;
            continue;
        }

        printf("Am here with an object of type %u\n", type);
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
    printf("objects read: %u, num of objects: %u\n", objects_read, num_of_objects);
    assert(objects_read == num_of_objects);
    printf("Current offset: %ld\n", ftell(bnk_file));

    for (uint32_t i = 0; i < sounds->length; i++) {
        printf("sound object id: %u, source id: %u, file id: %u\n", sounds->objects[i].sound_object_id, sounds->objects[i].source_id, sounds->objects[i].file_id);
    }

    for (uint32_t i = 0; i < events->length; i++) {
        printf("Self id of all event objects: %u\n", events->objects[i].self_id);
    }
    printf("amount of event ids: %u\n", events->length);
    printf("amount of event actions: %u, amount of sounds: %u\n", event_actions->length, sounds->length);
    for (uint32_t i = 0; i < event_actions->length; i++) {
        printf("event action sound object ids: %u\n", event_actions->objects[i].sound_object_id);
    }

    fclose(bnk_file);
    return 0;
}


int main(int argc, char* argv[])
{
    if (argc != 4) {
        fprintf(stderr, "Usage: %s path/to/binfile.bin  path/to/bnkfile.bnk  path/to/wpkfile.wpk\n Example: %s Annie/skin0.bin Annie/annie_base_vo_events.bnk Annie/annie_base_vo_audio.wpk\n", argv[0], argv[0]);
        exit(EXIT_FAILURE);
    }
    struct string_hashes* read_strings = parse_bin_file(argv[1]);
    char* bnk_path = argv[2];
    char* wpk_path = argv[3];

    struct sound_section sounds = {0};
    struct event_action_section event_actions = {0};
    struct event_section events = {0};
    struct random_container_section random_containers = {0};
    struct switch_container_section switch_containers = {0};
    struct actor_mixer_section actor_mixers = {0};


    parse_bnk_file(bnk_path, &sounds, &event_actions, &events, &random_containers, &switch_containers, &actor_mixers);

    for (uint32_t i = 0; i < read_strings->amount; i++) {
        printf("hashes[%u]: %u\n", i, read_strings->pairs[i].hash);
        printf("this is the hash for string \"%s\"\n", read_strings->pairs[i].string);
    }

    struct string_hashes string_files = {0, sounds.length * 2, malloc(sounds.length * 2 * sizeof(struct string_hash))};
    printf("allocated amount: %d\n", sounds.length * 2);

    printf("amount: %u\n", read_strings->amount);
    for (uint32_t i = 0; i < read_strings->amount; i++) {
        uint32_t hash = read_strings->pairs[i].hash;
        printf("hashes[%u]: %u, string: %s\n", i, read_strings->pairs[i].hash, read_strings->pairs[i].string);
        for (uint32_t j = 0; j < events.length; j++) {
            if (events.objects[j].self_id == hash) {
                for (int k = 0; k < events.objects[j].event_amount; k++) {
                    uint32_t event_id = events.objects[j].event_ids[k];
                    for (uint32_t l = 0; l < event_actions.length; l++) {
                        if (event_actions.objects[l].self_id == event_id) {
                            if (event_actions.objects[l].type == 4) {
                                uint32_t sound_object_id = event_actions.objects[l].sound_object_id;
                                printf("event type: %d\n", event_actions.objects[l].type);
                                for (uint32_t m = 0; m < sounds.length; m++) {
                                    if (sounds.objects[m].is_streamed && (sounds.objects[m].sound_object_id == sound_object_id || sounds.objects[m].self_id == sound_object_id)) {
                                        printf("Found one!\n");
                                        printf("Hash %u of string %s belongs to file \"%u.wem\".\n", hash, read_strings->pairs[i].string, sounds.objects[m].file_id);
                                        string_files.pairs[string_files.amount].hash = sounds.objects[m].file_id;
                                        string_files.pairs[string_files.amount].string = read_strings->pairs[i].string;
                                        string_files.amount++;
                                        printf("amount: %u\n", string_files.amount);
                                    }
                                }
                                for (uint32_t m = 0; m < random_containers.length; m++) {
                                    if (random_containers.objects[m].switch_container_id == sound_object_id) {
                                        // printf("sound object id that was found: %u\n", sound_object_id);
                                        for (uint32_t n = 0; n < random_containers.objects[m].sound_id_amount; n++) {
                                            for (uint32_t o = 0; o < sounds.length; o++) {
                                                if (sounds.objects[o].is_streamed && (random_containers.objects[m].sound_ids[n] == sounds.objects[o].self_id || random_containers.objects[m].sound_ids[n] == sounds.objects[o].sound_object_id)) {
                                                    printf("Found one precisely here.\n");
                                                    printf("Hash %u of string %s belongs to file \"%u.wem\".\n", hash, read_strings->pairs[i].string, sounds.objects[o].file_id);
                                                    string_files.pairs[string_files.amount].hash = sounds.objects[o].file_id;
                                                    string_files.pairs[string_files.amount].string = read_strings->pairs[i].string;
                                                    string_files.amount++;
                                                    printf("amount: %u\n", string_files.amount);
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
                                                            if (sounds.objects[p].is_streamed && (sounds.objects[p].sound_object_id == actor_mixers.objects[m].sound_object_ids[o] || (actor_mixers.objects[m].sound_object_id_amount == 1 && sounds.objects[p].self_id == actor_mixers.objects[m].sound_object_ids[o]))) {
                                                                printf("got one here!! YES, belongs to %u\n", sounds.objects[p].file_id);
                                                                printf("Hash %u of string %s belongs to file \"%u.wem\".\n", hash, read_strings->pairs[i].string, sounds.objects[p].file_id);
                                                                string_files.pairs[string_files.amount].hash = sounds.objects[p].file_id;
                                                                string_files.pairs[string_files.amount].string = read_strings->pairs[i].string;
                                                                string_files.amount++;
                                                                printf("amount: %u\n", string_files.amount);
                                                            // }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            } else if (event_actions.objects[l].type == 25) {
                                printf("event action sounds object id here: %u\n", event_actions.objects[l].sound_object_id);
                                for (uint32_t m = 0; m < switch_containers.length; m++) {
                                    if (event_actions.objects[l].switch_group_id == switch_containers.objects[m].switch_group_id) {
                                        for (uint32_t n = 0; n < random_containers.length; n++) {
                                            if (random_containers.objects[n].switch_container_id == switch_containers.objects[m].self_id) {
                                                for (uint32_t o = 0; o < random_containers.objects[n].sound_id_amount; o++) {
                                                    for (uint32_t p = 0; p < sounds.length; p++) {
                                                        if (random_containers.objects[n].sound_ids[o] == sounds.objects[p].self_id) {
                                                            printf("Found one down here!\n");
                                                            printf("Hash %u of string %s belongs to file \"%u.wem\".\n", hash, read_strings->pairs[i].string, sounds.objects[p].file_id);
                                                            string_files.pairs[string_files.amount].hash = sounds.objects[p].file_id;
                                                            string_files.pairs[string_files.amount].string = read_strings->pairs[i].string;
                                                            string_files.amount++;
                                                            printf("amount: %u\n", string_files.amount);
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


    extract_wpk_file(wpk_path, &string_files, "extracttest");
}
