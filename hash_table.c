/* hash_table.c */

#include "hash_table.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Hash function */
unsigned int hash_function(const char *str, int table_size) {
    unsigned int hash = 0;
    while (*str)
        hash = (hash << 5) + (unsigned char)(*str++);
    return hash % table_size;
}

/* Create a new hash table */
HashTable* create_hash_table(void) {
    HashTable *table = malloc(sizeof(HashTable));
    if (!table) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    table->size = INITIAL_HASH_SIZE;
    table->count = 0;
    table->buckets = calloc(table->size, sizeof(WordCount *));
    if (!table->buckets) {
        perror("calloc");
        free(table);
        exit(EXIT_FAILURE);
    }
    return table;
}

/* Resize the hash table */
void resize_hash_table(HashTable *table) {
    int new_size = table->size * 2;
    WordCount **new_buckets = calloc(new_size, sizeof(WordCount *));
    if (!new_buckets) {
        perror("calloc");
        exit(EXIT_FAILURE);
    }

    /* Rehash all existing entries */
    for (int i = 0; i < table->size; i++) {
        WordCount *node = table->buckets[i];
        while (node) {
            WordCount *next = node->next;
            unsigned int hash = hash_function(node->word, new_size);
            node->next = new_buckets[hash];
            new_buckets[hash] = node;
            node = next;
        }
    }

    free(table->buckets);
    table->buckets = new_buckets;
    table->size = new_size;
}

/* Insert or update a word in the hash table */
void insert_or_update_word(HashTable *table, const char *word, int count) {
    if ((double)table->count / table->size > LOAD_FACTOR_THRESHOLD) {
        resize_hash_table(table);
    }

    unsigned int hash = hash_function(word, table->size);
    WordCount *node = table->buckets[hash];
    while (node) {
        if (strcmp(node->word, word) == 0) {
            node->count += count;
            return;
        }
        node = node->next;
    }

    WordCount *new_node = malloc(sizeof(WordCount));
    if (!new_node) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    new_node->word = strdup(word);
    if (!new_node->word) {
        perror("strdup");
        free(new_node);
        exit(EXIT_FAILURE);
    }
    new_node->count = count;
    new_node->next = table->buckets[hash];
    table->buckets[hash] = new_node;
    table->count++;
}

/* Insert a word with count=1 */
void insert_word(HashTable *table, const char *word) {
    insert_or_update_word(table, word, 1);
}

/* Free the hash table */
void free_hash_table(HashTable *table) {
    for (int i = 0; i < table->size; i++) {
        WordCount *node = table->buckets[i];
        while (node) {
            WordCount *temp = node;
            node = node->next;
            free(temp->word);
            free(temp);
        }
    }
    free(table->buckets);
    free(table);
}

/* Comparison function for qsort */
int compare_counts(const void *a, const void *b) {
    WordCount *wc1 = *(WordCount **)a;
    WordCount *wc2 = *(WordCount **)b;
    return wc2->count - wc1->count;
}
