



#define INITIAL_HASH_SIZE 16
#define LOAD_FACTOR_THRESHOLD 0.75


typedef struct WordCount {
    char *word;
    int count;
    struct WordCount *next;
} WordCount;

typedef struct HashTable {
    WordCount **buckets;
    int size;
    int count;
} HashTable;

/* Hash Table Functions */
unsigned int hash_function(const char *str, int table_size);
HashTable* create_hash_table(void);
void resize_hash_table(HashTable *table);
void insert_or_update_word(HashTable *table, const char *word, int count);
void insert_word(HashTable *table, const char *word);
void free_hash_table(HashTable *table);

/* Comparison Function for qsort */
int compare_counts(const void *a, const void *b);

