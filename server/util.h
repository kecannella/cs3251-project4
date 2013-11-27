#define HASH_LENGTH 32

#define forKeyInHashmap(map, i, key) \
    for (i = 0; i < map->capacity; i++ ) \
        if ((key = map->entries[i].key) != NULL) \

#define LIST    1
#define DIFF    2
#define PULL    3
#define CAP     4
#define LEAVE   5

typedef char MessageType;

typedef struct {
    char *key;
    char *value;
} MapEntry;

typedef struct {
    MapEntry *entries;
    size_t capacity;
    size_t load;
} Hashmap;

typedef struct node{
	int weight;
	void *data;
	struct node *next;
} priority_node;

typedef struct {
	priority_node *head;
} priority_list;

Hashmap *newHashmap(size_t);
void freeHashmap(Hashmap *map, void (*)(void *), void (*)(void *));
char *getFromHashmap(Hashmap *, char *);
void putInHashmap(Hashmap *, char *, char *);
void growHashmap(Hashmap *, size_t);

priority_list *newPriorityList();
void freePriorityList(priority_list *, void (*)(void *));
void *getFromPriorityList(priority_list *);
void putInPriorityList(priority_list *, void *, int);
void *printPriorityList(priority_list *, void(*)(void *));

char *hashFile(FILE *);
void createIndex(Hashmap *, DIR *);
void createPriorityIndex(priority_list *, DIR *);
void exitIfError(int, const char *);
void exitThreadIfError(int, const char *);





