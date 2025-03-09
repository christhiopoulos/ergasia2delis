#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h> 
#include "hash_table.h"

#define MAX_WORD_LENGTH 100

int main() {


    // Measure start time
    struct timeval start_time, end_time;
    if (gettimeofday(&start_time, NULL) == -1) {
        perror("gettimeofday start_time");
        return 1;
    }

    // Create the hash table
    HashTable *hash_table = create_hash_table();
    if (!hash_table) {
        fprintf(stderr, "Failed to create hash table.\n");
        return 1;
    }

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), stdin) != NULL) {
        buffer[strcspn(buffer, "\n")] = '\0'; // Remove newline
        if (strlen(buffer) > 0) {
            insert_word(hash_table, buffer);
        }
    }

    // Output word counts
    for (int i = 0; i < hash_table->size; i++) {
        WordCount *node = hash_table->buckets[i];
        while (node) {
            printf("%s %d\n", node->word, node->count);
            node = node->next;
        }
    }

    // Flush stdout to ensure all word counts are sent
    fflush(stdout);

    // Measure end time
    if (gettimeofday(&end_time, NULL) == -1) {
        perror("gettimeofday end_time");
    }

    // Calculate elapsed time in seconds with microsecond precision
    double elapsed_time = (end_time.tv_sec - start_time.tv_sec) +
                          (end_time.tv_usec - start_time.tv_usec) / 1e6;

    // Send timing information with a special prefix
    printf("TIME %.6f\n", elapsed_time);
    fflush(stdout); // Ensure the timing info is sent

    // Notify the root process
    if (kill(getppid(), SIGUSR2) == -1) {
        perror("kill SIGUSR2");
        return 1;
    }

    // Clean up
    free_hash_table(hash_table);

    return 0;
}
