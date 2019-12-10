#define __USE_MINGW_ANSI_STDIO
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <assert.h>
#include <string.h>
#include <byteswap.h>

#include "defs.h"
#include "general_utils.h"
#include "wpk.h"

struct bin_strings {
    uint32_t amount;
    char** strings;
};

struct bin_strings* parse_bin_file(char* bin_path)
{
    FILE* bin_file = fopen(bin_path, "rb");
    if (!bin_file) {
        fprintf(stderr, "Error: Failed to open \"%s\".\n", bin_path);
        exit(EXIT_FAILURE);
    }

    struct bin_strings* saved_strings = calloc(1, sizeof(struct bin_strings));

    while (!feof(bin_file)) {
        int a, b, c, d;
        if ((a = getc(bin_file)) ==  0x84 && (b = getc(bin_file)) == 0xe3 && (c = getc(bin_file)) == 0xd8 && (d = getc(bin_file)) == 0x12) {
            fseek(bin_file, 6, SEEK_CUR);
            uint32_t amount;
            fread(&amount, 4, 1, bin_file);
            printf("amount: %u\n", amount);
            uint32_t current_amount = saved_strings->amount;
            saved_strings->amount += amount;
            if (!saved_strings->strings)
                saved_strings->strings = malloc(amount * sizeof(char*));
            else
                saved_strings->strings = realloc(saved_strings->strings, saved_strings->amount * sizeof(char*));

            for (; current_amount < saved_strings->amount; current_amount++) {
                uint16_t length;
                fread(&length, 2, 1, bin_file);
                saved_strings->strings[current_amount] = malloc(length + 1);
                fread(saved_strings->strings[current_amount], 1, length, bin_file);
                saved_strings->strings[current_amount][length] = '\0';
                printf("saved string \"%s\"\n", saved_strings->strings[current_amount]);
            }
        }
        // printf("ints: %X %d, %X %d, %X %d, %X %d\n", a, a, b, b, c, c, d, d);
    }

    for (uint32_t i = 0; i < saved_strings->amount; i++) {
        printf("string at position %u: \"%s\".\n", i, saved_strings->strings[i]);
    }

    fclose(bin_file);
    return saved_strings;
}

uint32_t fnv_1_hash(const char* input)
{
    uint32_t hash = 0x811c9dc5;
    const char* c = input;
    while (*c != 0) {
        hash *= 0x01000193;
        hash ^= (*c > 64 && *c < 91) ? *c + 32 : *c;
        c++;
    }

    return hash;
}

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

struct sound {
    uint32_t self_id;
    uint32_t file_id;
    uint32_t source_id;
    uint32_t sound_object_id;
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

struct random_container_section {
    uint32_t length;
    uint32_t allocated_length;
    struct random_container* random_containers;
};

struct switch_container_section {
    uint32_t length;
    uint32_t allocated_length;
    struct switch_container* switch_containers;
};

struct sound_section {
    uint32_t length;
    uint32_t allocated_length;
    struct sound* sounds;
};

struct event_action_section {
    uint32_t length;
    uint32_t allocated_length;
    struct event_action* event_actions;
};

struct event_section {
    uint32_t length;
    uint32_t allocated_length;
    struct event* events;
};

void add_random_container(struct random_container_section* random_containers, struct random_container* object)
{
    if (random_containers->allocated_length == 0) {
        random_containers->random_containers = malloc(16 * sizeof(struct random_container));
        random_containers->length = 0;
        random_containers->allocated_length = 16;
    } else if (random_containers->length == random_containers->allocated_length) {
        random_containers->random_containers = realloc(random_containers->random_containers, (random_containers->allocated_length + (random_containers->allocated_length >> 1)) * sizeof(struct random_container));
        random_containers->allocated_length += random_containers->allocated_length >> 1;
    }

    memcpy(&random_containers->random_containers[random_containers->length], object, sizeof(struct random_container));
    random_containers->length++;
}

void add_switch_container(struct switch_container_section* switch_containers, struct switch_container* object)
{
    if (switch_containers->allocated_length == 0) {
        switch_containers->switch_containers = malloc(16 * sizeof(struct switch_container));
        switch_containers->length = 0;
        switch_containers->allocated_length = 16;
    } else if (switch_containers->length == switch_containers->allocated_length) {
        switch_containers->switch_containers = realloc(switch_containers->switch_containers, (switch_containers->allocated_length + (switch_containers->allocated_length >> 1)) * sizeof(struct switch_container));
        switch_containers->allocated_length += switch_containers->allocated_length >> 1;
    }

    memcpy(&switch_containers->switch_containers[switch_containers->length], object, sizeof(struct switch_container));
    switch_containers->length++;
}

void add_sound(struct sound_section* sounds, struct sound* object)
{
    if (sounds->allocated_length == 0) {
        sounds->sounds = malloc(16 * sizeof(struct sound));
        sounds->length = 0;
        sounds->allocated_length = 16;
    } else if (sounds->length == sounds->allocated_length) {
        sounds->sounds = realloc(sounds->sounds, (sounds->allocated_length + (sounds->allocated_length >> 1)) * sizeof(struct sound));
        sounds->allocated_length += sounds->allocated_length >> 1;
    }

    memcpy(&sounds->sounds[sounds->length], object, sizeof(struct sound));
    sounds->length++;
}

void add_event_action(struct event_action_section* event_actions, struct event_action* object)
{
    if (event_actions->allocated_length == 0) {
        event_actions->event_actions = malloc(16 * sizeof(struct event_action));
        event_actions->length = 0;
        event_actions->allocated_length = 16;
    } else if (event_actions->length == event_actions->allocated_length) {
        event_actions->event_actions = realloc(event_actions->event_actions, (event_actions->allocated_length + (event_actions->allocated_length >> 1)) * sizeof(struct event_action));
        event_actions->allocated_length += event_actions->allocated_length >> 1;
    }

    memcpy(&event_actions->event_actions[event_actions->length], object, sizeof(struct event_action));
    event_actions->length++;
}

void add_event(struct event_section* events, struct event* object)
{
    if (events->allocated_length == 0) {
        events->events = malloc(16 * sizeof(struct event));
        events->length = 0;
        events->allocated_length = 16;
    } else if (events->length == events->allocated_length) {
        events->events = realloc(events->events, (events->allocated_length + (events->allocated_length >> 1)) * sizeof(struct event));
        events->allocated_length += events->allocated_length >> 1;
    }

    memcpy(&events->events[events->length], object, sizeof(struct sound));
    events->length++;
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
            to_seek = 35; //111;
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
    add_random_container(random_containers, &new_random_container_object);

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
    add_switch_container(switch_containers, &new_switch_container_object);

    return 0;
}

int read_sound_object(FILE* bnk_file, uint32_t object_length, struct sound_section* sounds)
{
    uint32_t initial_position = ftell(bnk_file);
    struct sound new_sound_object;
    fread(&new_sound_object.self_id, 4, 1, bnk_file);
    fseek(bnk_file, 5, SEEK_CUR);
    fread(&new_sound_object.file_id, 4, 1, bnk_file);
    fread(&new_sound_object.source_id, 4, 1, bnk_file);
    fseek(bnk_file, 8, SEEK_CUR);
    fread(&new_sound_object.sound_object_id, 4, 1, bnk_file);
    fseek(bnk_file, initial_position + object_length, SEEK_SET);
    // printf("offset now: %ld\n", ftell(bnk_file));

    add_sound(sounds, &new_sound_object);

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

    add_event_action(event_actions, &new_event_action_object);

    return 0;
}

int read_event_object(FILE* bnk_file, struct event_section* events)
{
    struct event new_event_object;
    fread(&new_event_object.self_id, 4, 1, bnk_file);
    fread(&new_event_object.event_amount, 1, 1, bnk_file);
    new_event_object.event_ids = malloc(new_event_object.event_amount * 4);
    fread(new_event_object.event_ids, 4, new_event_object.event_amount, bnk_file);

    add_event(events, &new_event_object);

    return 0;
}


int parse_bnk_file(char* path, struct sound_section* sounds, struct event_action_section* event_actions, struct event_section* events, struct random_container_section* random_containers, struct switch_container_section* switch_containers)
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
        if (type != 2 && type != 3 && type != 4 && type != 5 && type != 6) {
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
        }

        objects_read++;
    }
    printf("objects read: %u, num of objects: %u\n", objects_read, num_of_objects);
    assert(objects_read == num_of_objects);
    printf("Current offset: %ld\n", ftell(bnk_file));

    for (uint32_t i = 0; i < sounds->length; i++) {
        printf("sound object id: %u, source id: %u, file id: %u\n", sounds->sounds[i].sound_object_id, sounds->sounds[i].source_id, sounds->sounds[i].file_id);
    }

    for (uint32_t i = 0; i < events->length; i++) {
        printf("Self id of all event objects: %u\n", events->events[i].self_id);
    }
    printf("amount of event ids: %u\n", events->length);
    printf("amount of event actions: %u, amount of sounds: %u\n", event_actions->length, sounds->length);
    for (uint32_t i = 0; i < event_actions->length; i++) {
        printf("event action sound object ids: %u\n", event_actions->event_actions[i].sound_object_id);
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
    struct bin_strings* read_strings = parse_bin_file(argv[1]);
    char* bnk_path = argv[2];
    char* wpk_path = argv[3];

    struct sound_section sounds = {0};
    struct event_action_section event_actions = {0};
    struct event_section events = {0};
    struct random_container_section random_containers = {0};
    struct switch_container_section switch_containers= {0};


    parse_bnk_file(bnk_path, &sounds, &event_actions, &events, &random_containers, &switch_containers);

    uint32_t hashes[read_strings->amount];
    for (uint32_t i = 0; i < read_strings->amount; i++) {
        hashes[i] = fnv_1_hash(read_strings->strings[i]);
        printf("hashes[%u]: %u\n", i, hashes[i]);
        printf("this is the hash for string \"%s\"\n", read_strings->strings[i]);
    }

    struct string_file_hash string_files[sounds.length * 2];

    uint32_t index = 0;
    printf("amount: %u\n", read_strings->amount);
    for (uint32_t i = 0; i < read_strings->amount; i++) {
        uint32_t hash = hashes[i];
        printf("hashes[%u]: %u, string: %s\n", i, hashes[i], read_strings->strings[i]);
        for (uint32_t j = 0; j < events.length; j++) {
            if (events.events[j].self_id == hash) {
                for (int k = 0; k < events.events[j].event_amount; k++) {
                    uint32_t event_id = events.events[j].event_ids[k];
                    for (uint32_t l = 0; l < event_actions.length; l++) {
                        if (event_actions.event_actions[l].self_id == event_id) {
                            if (event_actions.event_actions[l].type == 4) {
                                uint32_t sound_object_id = event_actions.event_actions[l].sound_object_id;
                                printf("event type: %d\n", event_actions.event_actions[l].type);
                                for (uint32_t m = 0; m < sounds.length; m++) {
                                    printf("sound object id to find: %u, the one currently: %u\n", sound_object_id, sounds.sounds[m].sound_object_id);
                                    if (sounds.sounds[m].sound_object_id == sound_object_id || sounds.sounds[m].self_id == sound_object_id) {
                                        printf("Found one!\n");
                                        printf("Hash %u of string %s belongs to file \"%u.wem\".\n", hash, read_strings->strings[i], sounds.sounds[m].file_id);
                                        string_files[index].file_hash = sounds.sounds[m].file_id;
                                        string_files[index].string = read_strings->strings[i];
                                        index++;
                                        printf("index: %u\n", index);
                                    }
                                }
                            } else if (event_actions.event_actions[l].type == 25) {
                                printf("event action sounds object id here: %u\n", event_actions.event_actions[l].sound_object_id);
                                for (uint32_t m = 0; m < switch_containers.length; m++) {
                                    if (event_actions.event_actions[l].switch_group_id == switch_containers.switch_containers[m].switch_group_id) {
                                        for (uint32_t n = 0; n < random_containers.length; n++) {
                                            if (random_containers.random_containers[n].switch_container_id == switch_containers.switch_containers[m].self_id) {
                                                for (uint32_t o = 0; o < random_containers.random_containers[n].sound_id_amount; o++) {
                                                    for (uint32_t p = 0; p < sounds.length; p++) {
                                                        if (random_containers.random_containers[n].sound_ids[o] == sounds.sounds[p].self_id) {
                                                            printf("Found one down here!\n");
                                                            printf("Hash %u of string %s belongs to file \"%u.wem\".\n", hash, read_strings->strings[i], sounds.sounds[p].file_id);
                                                            string_files[index].file_hash = sounds.sounds[p].file_id;
                                                            string_files[index].string = read_strings->strings[i];
                                                            index++;
                                                            printf("index: %u\n", index);
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

        // for (uint32_t j = 0; j < random_containers.length; j++) {
        //     for (uint32_t k = 0; k < event_actions.length; k++) {
        //         if (event_actions.event_actions[k].sound_object_id == random_containers.random_containers[j].event_action_sound_object_id) {
        //             printf("type of event action: %d\n", event_actions.event_actions[k].type);
        //             for (uint32_t l = 0; l < sounds.length; l++) {
        //                 if (sounds.sounds[l].sound_object_id == random_containers.random_containers[j].sound_sound_object_id) {
        //                     printf("found one down here!\n");
        //                     printf("Hash %u of string %s belongs to file \"%u.wem\".\n", )
        //                 }
        //             }

        //         }
        //     }
        // }
    }


    extract_wpk_file(wpk_path, string_files, index, "extracttest");
}
