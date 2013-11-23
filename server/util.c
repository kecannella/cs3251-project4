#include <dirent.h>
#include <openssl/evp.h>
#include <pthread.h>
#include <string.h>

#include "util.h"

#define LOAD_FACTOR 0.5d

char *hashFile(FILE *file) {
    unsigned char *md_value = malloc(HASH_LENGTH);
    exitIfError(md_value == NULL, "mallocing during file hashing");
    EVP_MD_CTX mdctx;
    EVP_MD_CTX_init(&mdctx);
    const EVP_MD *md = EVP_sha256();
    EVP_DigestInit_ex(&mdctx, md, NULL);
    int bytes;
    unsigned char data[4096];
    while ((bytes = fread(data, 1, 4096, file)) != 0) {
        exitIfError(ferror(file), "reading data from file during hashing");
        EVP_DigestUpdate(&mdctx, data, bytes);
    }
    EVP_DigestFinal_ex(&mdctx, md_value, NULL);
    EVP_MD_CTX_cleanup(&mdctx);
    return md_value;
}
void createIndex(Hashmap *map, DIR *directory) {
    if (directory == NULL) {
        printf("ERROR");
    }
    struct dirent *ent;
    while ((ent = readdir(directory)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
            continue;
        }
        FILE *file = fopen(ent->d_name, "rb");
        exitIfError(ferror(file), "opening file during indexing");
        char *hash = hashFile(file);
        exitIfError(fclose(file) == EOF, "closing file during indexing");
        putInHashmap(map, hash, ent->d_name);
    }
}

Hashmap *newHashmap(size_t capacity) {
    Hashmap *map = malloc(sizeof(Hashmap));
    exitIfError(map == NULL, "mallocing during hashmap creation");
    map->capacity = capacity;
    map->entries = calloc(capacity, sizeof(MapEntry));
    exitIfError(map->entries == NULL, "callocing during hashmap creation");
    map->load = 0;
    return map;
}

void freeHashmap(Hashmap *map, void (*freeKeyFunc)(void *), void (*freeValueFunc)(void *)) {
    size_t i;
    for (i = 0; i < map->capacity; i++) {
        if (freeKeyFunc != NULL) {
            freeKeyFunc(map->entries[i].key);
        }
        if (freeValueFunc != NULL) {
            freeValueFunc(map->entries[i].value);
        }
    }
    free(map->entries);
    free(map);
}

char *getFromHashmap(Hashmap *map, char *key) {
    size_t index = *(size_t *)key % map->capacity; //first n bits of hash
    MapEntry *entry = map->entries + index;
    while (entry->key != NULL && strncmp(entry->key, key, HASH_LENGTH) != 0) {
        entry++;
        if (entry >= map->entries + map->capacity) {
            entry = map->entries;
        }
    }
    return entry->value;
}

void putInHashmap(Hashmap *map, char *key, char *value) {
    if (map->load >= (map->capacity * LOAD_FACTOR)) {
        growHashmap(map, map->capacity * 2);
    }
    size_t index = *(size_t *)key % map->capacity; //first n bits of hash
    MapEntry *entry = map->entries + index;
    while (entry->key != NULL && strncmp(entry->key, key, HASH_LENGTH) != 0) {
        entry++;
        if (entry >= map->entries + map->capacity) {
            entry = map->entries;
        }
    }
    if (entry->key == NULL) {
        map->load++;
    }
    entry->key = key;
    entry->value = value;
}

void growHashmap(Hashmap *map, size_t newCapacity) {
    map->entries = realloc(map->entries, newCapacity * sizeof(MapEntry));
    exitIfError(map->entries == NULL, "reallocing during hashmap resize");
    memset(map->entries + map->capacity, 0, (newCapacity - map->capacity) * sizeof(MapEntry));
    size_t currIndex;
    for (currIndex = 0; currIndex < map->capacity; currIndex++) {
        MapEntry *oldEntry = map->entries + currIndex;
        if (oldEntry->key == NULL) {
            continue;
        }
        size_t newIndex = *(size_t *)oldEntry->key % newCapacity;
        if (newIndex == currIndex) {  
            continue;
        }
        MapEntry *newEntry = map->entries + newIndex;
        while (newEntry->key != NULL) {
            newEntry++;
            if (newEntry >= map->entries + newCapacity) {
                newEntry = map->entries;
            }
        }
        newEntry->key = oldEntry->key;
        newEntry->value = oldEntry->value;
        oldEntry->key = NULL;
        oldEntry->value = NULL;
    }
    map->capacity = newCapacity;
}

void exitIfError(int failIfTrue, const char *action) {
    if (failIfTrue) {
        fprintf(stderr, "Fatal error while %s: ", action);
        perror(NULL);
        exit(EXIT_FAILURE);
    }
}