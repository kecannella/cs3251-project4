#include <dirent.h>
#include <openssl/evp.h>
#include <pthread.h>
#include <string.h>
#include <sys/stat.h>

#include "util.h"

#define LOAD_FACTOR 0.5

char *hashFile(FILE *file) {
    unsigned char *md_value = malloc(HASH_LENGTH);
    exitIfError(md_value == NULL, "mallocing during file hashing");
    EVP_MD_CTX mdctx;
    EVP_MD_CTX_init(&mdctx);
    const EVP_MD *md = EVP_sha256();
    EVP_DigestInit_ex(&mdctx, md, NULL);
    int bytes;
    unsigned char data[8192];
    while ((bytes = fread(data, 1, 8192, file)) != 0) {
        exitIfError(ferror(file), "reading data from file during hashing");
        EVP_DigestUpdate(&mdctx, data, bytes);
    }
    EVP_DigestFinal_ex(&mdctx, md_value, NULL);
    EVP_MD_CTX_cleanup(&mdctx);
    /*int i;
    for (i = 0; i < HASH_LENGTH; i++) {
        printf("%02x", md_value[i]);
    }
    printf("\n");*/
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

char *getNextName(char *pos) {
	char *temp = strstr(pos,"<key>Location</key>");
    if (temp == NULL) return NULL; //reached end of tracks list
    char *name_start = strstr(temp, "<string>");
    if (name_start == NULL) return NULL;
    else name_start = name_start + 8;
    char *name_end = strstr(temp, "</string>");
    if (name_end == NULL) return NULL;
    char *name = calloc(name_end - name_start + 1, 1);
    exitIfError(name == NULL, "mallocing during itunes xml parsing");
    strncpy(name, name_start, name_end - name_start);
    
    char *token, *oldtoken;
    token = strstr(name,"/");
    while (token != NULL) {
    	oldtoken = token;
    	token = strstr(token + 1,"/");
    }
    char *file = calloc(name + strlen(name) - oldtoken, 1);
    exitIfError(file == NULL, "mallocing during itunes xml parsing");
    strncpy(file, oldtoken + 1, name + strlen(name) - oldtoken - 1);
    free(name);
    
    int length = strlen(file);
    char *space = strstr(file,"%20");
    while (space != NULL) {
    	*space = ' ';
    	while(space < file + length - 3) {
    		*(space + 1) = *(space + 3);
    		++space;
    	}
	   	*(file + length - 2) = '\0';
	   	*(file + length - 1) = '\0';
    	space = strstr(file,"%20");
    }
    
    return file;
}

char *getHashInDir (char *name, DIR *directory) {
	char *hash = NULL;
	struct dirent *ent;
	while ((ent = readdir(directory)) != NULL) {
		if (strcmp(ent->d_name, name) == 0) {
			FILE *file = fopen(ent->d_name, "rb");
			exitIfError(ferror(file), "opening file during indexing");
			hash = hashFile(file);
		}
	}
	rewinddir(directory);
	return hash;
}

int getNextPlayCount(char **pos) {
	char *end = strstr(*pos,"<key>Location</key>"); 
	char *temp = strstr(*pos,"<key>Play Count</key>"); 
	*pos = end;
    if (*pos != NULL) *pos += 19;	
    if (temp == NULL || (end != NULL && temp > end)) return 0; 
    char *count_start = strstr(temp, "<integer>"); 
    if (count_start == NULL || (end != NULL && count_start > end)) return 0;
    else count_start = count_start + 9;
    char *count_end = strstr(temp, "</integer>"); 
    if (count_end == NULL || (end != NULL && count_end > end)) return 0;
    char *count = calloc(count_end - count_start + 1, 1);
    strncpy(count, count_start, count_end - count_start);
    int num = atoi(count); 
    free(count);
    return num;
}

void createPriorityIndex(priority_list *list, DIR *directory, char *clientlist) {
    if (directory == NULL) {
        printf("ERROR");
    }
    
	FILE *file = fopen("iTunes Music Library.xml", "r");
	struct stat st;
	stat("iTunes Music Library.xml", &st);
	int size = st.st_size;
	
	char *itunes = calloc(size * sizeof(char) + 1, sizeof(char));
    exitIfError(itunes == NULL, "mallocing during PriorityList creation");
	
    int bytes = fread(itunes, 1, size, file);
    exitIfError(bytes != size, "reading data from itunes xml file");
    exitIfError(ferror(file), "reading data from itunes xml file");
    
    fclose(file);
    
    char *pos = itunes;
    int i;
    while (pos != NULL) {
		char *name = getNextName(pos);
		if (name == NULL) break;
		int count = getNextPlayCount(&pos);
		char *hash = getHashInDir(name, directory);
		
		if (hash != NULL) {
			char *clienthash = strstr(clientlist,hash);
			//if (clienthash != NULL) {
				printf("Putting (%d,%s) in the list\n",count,name);
				putInPriorityList(list, hash, count);
			//}
			//free(hash);
		}
		free(name);
    }
    
    free(itunes);

}

priority_list *newPriorityList() {
    priority_list *list = malloc(sizeof(priority_list));
    exitIfError(list == NULL, "mallocing during PriorityList creation");
    list->head = NULL;
    return list;
}

void freePriorityList(priority_list *list, void (*freeDataFunc)(void *)) {
    if (list != NULL) {
		while (list->head != NULL) {
			void *data = getFromPriorityList(list);
		    if (freeDataFunc != NULL) {
		        freeDataFunc(data);
		    }
		}
		free(list);
	}
}

void *getFromPriorityList(priority_list *list) {
	if (list->head == NULL) {
		return NULL;
	}
	else {
		priority_node *node = list->head;
		list->head = list->head->next;
		void *data = node->data;
		free(node);
		return data;
	}
}

void putInPriorityList(priority_list *list, void *data, int weight) {
    if (list != NULL) {
		priority_node *node = malloc(sizeof(priority_node));
		exitIfError(node == NULL, "mallocing during priority node creation");
		node->data = data;
		node->weight = weight;
		node->next = NULL;
		if (list->head == NULL || list->head->weight < weight) {
			node->next = list->head;
			list->head = node;
		}
		else {
			priority_node *current = list->head;
			while (current->next != NULL && current->next->weight > weight) {
				current = current->next;
			}
			priority_node *next = current->next;
			current->next = node;
			node->next = next;
		}
	}
}

void *printPriorityList(priority_list *list, void(*printDataFunc)(void *)) {
	if (list != NULL) {
		priority_node *current = list->head;
		while(current != NULL) {
			printf("%d\t", current->weight);
			if (printDataFunc != NULL) {
				printDataFunc(current->data);
			}
			printf("\n");
			current = current->next;
		}
		printf("\n");
	}
}


void exitIfError(int failIfTrue, const char *action) {
    if (failIfTrue) {
        fprintf(stderr, "Fatal error while %s: ", action);
        perror(NULL);
        exit(EXIT_FAILURE);
    }
}
