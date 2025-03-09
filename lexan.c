#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/times.h>
#include <sys/time.h>
#include "lexan.h"      



volatile sig_atomic_t usr1_count = 0;
volatile sig_atomic_t usr2_count = 0;
int num_splitters = 0;
int num_builders = 0;

// Signal handlers
void handle_usr1(int sig) {
    (void)sig;
    usr1_count++;
}

void handle_usr2(int sig) {
    (void)sig;
    usr2_count++;
}




int main(int argc, char *argv[]) {
    char *input_file = NULL, *exclusion_file = NULL, *output_file = NULL;
    int top_k = 0;

    // Variables for timing
    struct tms tb1, tb2;
    clock_t t1, t2;
    double ticspersec;
    double cpu_time, real_time;

    // Initialize ticspersec for CPU time
    ticspersec = (double) sysconf(_SC_CLK_TCK);

    // Record initial times
    t1 = times(&tb1);
    if (t1 == (clock_t)-1) {
        perror("times");
        return 1;
    }

    // Parse command-line arguments
    for (int i = 1; i < argc; i++) {
        // Check if the argument starts with '-'
        if (argv[i][0] == '-') {
            // Ensure the flag has at least two characters (e.g., '-i')
            if (strlen(argv[i]) < 2) {
                fprintf(stderr, "Invalid flag: %s\n", argv[i]);
                fprintf(stderr, "Usage: %s -i input_file -l num_splitters -m num_builders -t top_k -e exclusion_file -o output_file\n", argv[0]);
                return 1;
            }

            char flag = argv[i][1]; // Get the flag character


            // Assign values based on the flag
            switch (flag) {
                case 'i':
                    input_file = argv[++i];
                    break;
                case 'l':
                    num_splitters = atoi(argv[++i]);
                    if (num_splitters <= 0) {
                        fprintf(stderr, "Invalid number of splitters: %s\n", argv[i]);
                        return 1;
                    }
                    break;
                case 'm':
                    num_builders = atoi(argv[++i]);
                    if (num_builders <= 0) {
                        fprintf(stderr, "Invalid number of builders: %s\n", argv[i]);
                        return 1;
                    }
                    break;
                case 't':
                    top_k = atoi(argv[++i]);
                    if (top_k <= 0) {
                        fprintf(stderr, "Invalid top_k value: %s\n", argv[i]);
                        return 1;
                    }
                    break;
                case 'e':
                    exclusion_file = argv[++i];
                    break;
                case 'o':
                    output_file = argv[++i];
                    break;
                default:
                    fprintf(stderr, "Unknown flag: -%c\n", flag);
                    fprintf(stderr, "Usage: %s -i input_file -l num_splitters -m num_builders -t top_k -e exclusion_file -o output_file\n", argv[0]);
                    return 1;
            }
        } else {
            // Handle non-flag arguments, if necessary
            fprintf(stderr, "Unexpected argument: %s\n", argv[i]);
            fprintf(stderr, "Usage: %s -i input_file -l num_splitters -m num_builders -t top_k -e exclusion_file -o output_file\n", argv[0]);
            return 1;
        }
    }

    // Check for missing or invalid arguments
    if (!input_file || !exclusion_file || !output_file || num_splitters <= 0 || num_builders <= 0 || top_k <= 0) {
        fprintf(stderr, "Error: Missing or invalid arguments.\n");
        fprintf(stderr, "Usage: %s -i input_file -l num_splitters -m num_builders -t top_k -e exclusion_file -o output_file\n", argv[0]);
        return 1;
    }

    // Check if files can be opened
    FILE *test_fp;
    if ((test_fp = fopen(exclusion_file, "r")) == NULL) {
        fprintf(stderr, "Error: Exclusion file '%s' cannot be opened.\n", exclusion_file);
        perror("fopen exclusion_file");
        return 1;
    }
    fclose(test_fp);

    if ((test_fp = fopen(input_file, "r")) == NULL) {
        fprintf(stderr, "Error: Input file '%s' cannot be opened.\n", input_file);
        perror("fopen input_file");
        return 1;
    }
    fclose(test_fp);

    // Set up signal handlers
    signal(SIGUSR1, handle_usr1);
    signal(SIGUSR2, handle_usr2);
    // Allocate memory for PIDs and pipes
    pid_t *builder_pids = malloc(num_builders * sizeof(pid_t));
    pid_t *splitter_pids = malloc(num_splitters * sizeof(pid_t));
    int **splitter_to_builder_pipes = malloc(num_builders * sizeof(int *));
    int **builder_to_root_pipes = malloc(num_builders * sizeof(int *));

    if (!builder_pids || !splitter_pids || !splitter_to_builder_pipes || !builder_to_root_pipes) {
        perror("malloc");
        return 1;
    }

    // Allocate memory for start and end times of builders
    struct timeval *builder_start_times = malloc(num_builders * sizeof(struct timeval));
    struct timeval *builder_end_times = malloc(num_builders * sizeof(struct timeval));

    if (!builder_start_times || !builder_end_times) {
        perror("malloc");
        return 1;
    }

    // Create pipes for each builder
    for (int i = 0; i < num_builders; i++) {
        splitter_to_builder_pipes[i] = malloc(2 * sizeof(int));
        builder_to_root_pipes[i] = malloc(2 * sizeof(int));
        if (!splitter_to_builder_pipes[i] || !builder_to_root_pipes[i]) {
            perror("malloc");
            return 1;
        }

        if (pipe(splitter_to_builder_pipes[i]) == -1 || pipe(builder_to_root_pipes[i]) == -1) {
            perror("pipe");
            return 1;
        }
    }

    // Create builders
    for (int i = 0; i < num_builders; i++) {
        builder_pids[i] = fork();
        if (builder_pids[i] < 0) {
            perror("fork builder");
            return 1;
        }
        if (builder_pids[i] == 0) {
            // Child process (builder)
            // Redirect splitter_to_builder_pipes[i][0] to STDIN
            if (dup2(splitter_to_builder_pipes[i][0], STDIN_FILENO) == -1) {
                perror("dup2 STDIN");
                return 1;
            }
            // Redirect builder_to_root_pipes[i][1] to STDOUT
            if (dup2(builder_to_root_pipes[i][1], STDOUT_FILENO) == -1) {
                perror("dup2 STDOUT");
                return 1;
            }

            // Close all pipe file descriptors in the child
            for (int j = 0; j < num_builders; j++) {
                close(splitter_to_builder_pipes[j][0]);
                close(splitter_to_builder_pipes[j][1]);
                close(builder_to_root_pipes[j][0]);
                close(builder_to_root_pipes[j][1]);
            }

            execl("./builder", "builder", NULL);
            perror("execl builder");
            return 1;
        }
    }

    // Parent process: Close the read ends of splitter_to_builder_pipes and the write ends of builder_to_root_pipes
    for (int i = 0; i < num_builders; i++) {
        close(splitter_to_builder_pipes[i][0]);
        close(builder_to_root_pipes[i][1]);
    }

    // Create splitters
    for (int i = 0; i < num_splitters; i++) {
        splitter_pids[i] = fork();
        if (splitter_pids[i] < 0) {
            perror("fork splitter");
            return 1;
        }
        if (splitter_pids[i] == 0) {
            // Child process (splitter)
            // Prepare pipe_fds_str
            char pipe_fds_str[4096] = "";
            for (int j = 0; j < num_builders; j++) {
                char fd_str[12];
                snprintf(fd_str, sizeof(fd_str), "%d ", splitter_to_builder_pipes[j][1]);
                strcat(pipe_fds_str, fd_str);
            }

            // Convert splitter_id and num_builders to strings
            char splitter_id_str[12], num_builders_str[12];
            snprintf(splitter_id_str, sizeof(splitter_id_str), "%d", i);
            snprintf(num_builders_str, sizeof(num_builders_str), "%d", num_builders);

            // Close unused file descriptors in the child
            for (int j = 0; j < num_builders; j++) {
                close(splitter_to_builder_pipes[j][0]);
                close(builder_to_root_pipes[j][0]);
                close(builder_to_root_pipes[j][1]);
            }

            execl("./splitter", "splitter", splitter_id_str, input_file, exclusion_file, num_builders_str, pipe_fds_str, NULL);
            perror("execl splitter");
            return 1;
        }
    }

    // Parent process: Close the write ends of splitter_to_builder_pipes, as they are handled by splitters
    for (int i = 0; i < num_builders; i++) {
        close(splitter_to_builder_pipes[i][1]);
    }

    // Wait for all splitters to finish
    for (int i = 0; i < num_splitters; i++) {
        while (waitpid(splitter_pids[i], NULL, 0) == -1 );
    }

    // Create the hash table
    HashTable *hash_table = create_hash_table();
    if (!hash_table) {
        fprintf(stderr, "Failed to create hash table.\n");
        return 1;
    }
    int total_words = 0;
    int total_non_excluded_words = 0;

    // Allocate array to store elapsed times from builders
    double *builder_elapsed_times = calloc(num_builders, sizeof(double));
    if (!builder_elapsed_times) {
        perror("calloc builder_elapsed_times");
        return 1;
    }

    // Collect results from builders
    for (int i = 0; i < num_builders; i++) {
        FILE *fp = fdopen(builder_to_root_pipes[i][0], "r");
        if (!fp) {
            perror("fdopen");
            return 1;
        }

        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            // Check if the line starts with "TIME"
            if (strncmp(line, "TIME ", 5) == 0) {
                double time_taken;
                if (sscanf(line + 5, "%lf", &time_taken) == 1) {
                    builder_elapsed_times[i] = time_taken;
                } else {
                    fprintf(stderr, "Builder %d sent malformed TIME line: %s", i, line);
                }
            } else {
                // Process as word count
                char word[100];
                int count;
                if (sscanf(line, "%s %d", word, &count) == 2) {
                    insert_or_update_word(hash_table, word, count);
                    total_non_excluded_words += count; // Update total word count
                } else {
                    fprintf(stderr, "Builder %d sent malformed word count line: %s", i, line);
                }
            }
        }

        fclose(fp); // Close the read-end of the pipe after reading
    }

    // Close all read ends of builder_to_root_pipes
    for (int i = 0; i < num_builders; i++) {
        close(builder_to_root_pipes[i][0]);
    }

    // Wait for all builders to finish 
    for (int i = 0; i < num_builders; i++) {
        while (waitpid(builder_pids[i], NULL, 0) == -1 );

    }

    // Calculate total unique words
    total_words = hash_table->count;

    if (total_words == 0) {
        fprintf(stderr, "No words to process.\n");
        // Create an empty output file
        FILE *out_fp = fopen(output_file, "w");
        if (!out_fp) {
            perror("fopen output_file");
            return 1;
        }
        fclose(out_fp);

        // Free allocated resources before exiting
        for (int i = 0; i < num_builders; i++) {
            free(splitter_to_builder_pipes[i]);
            free(builder_to_root_pipes[i]);
        }

        free(builder_pids);
        free(splitter_pids);
        free(splitter_to_builder_pipes);
        free(builder_to_root_pipes);
        free_hash_table(hash_table);
        free(builder_start_times);
        free(builder_end_times);
        free(builder_elapsed_times);
        return 0;
    }

    // Create an array of WordCount pointers for sorting
    WordCount **word_array = malloc(total_words * sizeof(WordCount *));
    if (!word_array) {
        perror("malloc word_array");
        return 1;
    }

    int index = 0;
    for (int i = 0; i < hash_table->size; i++) {
        WordCount *node = hash_table->buckets[i];
        while (node) {
            word_array[index++] = node;
            node = node->next;
        }
    }

    // Sort the word_array based on counts in descending order
    qsort(word_array, total_words, sizeof(WordCount *), compare_counts);

    // Write the top_k words to the output file with fraction format
    FILE *out_fp = fopen(output_file, "w");
    if (!out_fp) {
        perror("fopen output_file");
        return 1;
    }

    for (int i = 0; i < top_k && i < total_words; i++) {
        fprintf(out_fp, "%s: %d/%d\n", word_array[i]->word, word_array[i]->count, total_non_excluded_words);
        printf("%s: %d/%d\n", word_array[i]->word, word_array[i]->count, total_non_excluded_words); // Also print to screen
    }
    fclose(out_fp);

    // Print the elapsed time reported by each builder
    for (int i = 0; i < num_builders; i++) {
        if (builder_elapsed_times[i] > 0) {
            printf("Builder %d completed in %.6f seconds.\n", i, builder_elapsed_times[i]);
        } else {
            printf("Builder %d did not report timing information.\n", i);
        }
    }

    // Print the number of signals received
    printf("Total SIGUSR1 signals received: %d\n", usr1_count);
    printf("Total SIGUSR2 signals received: %d\n", usr2_count);

    // Record end times
    t2 = times(&tb2);
    if (t2 == (clock_t)-1) {
        perror("times");
        return 1;
    }

    // Calculate CPU and real time
    cpu_time = (double)((tb2.tms_utime + tb2.tms_stime ) -
                        (tb1.tms_utime + tb1.tms_stime )) / ticspersec;
    real_time = (double)((t2) - (t1)) / ticspersec;

    // Print the timing results
    printf("Run time was %lf sec (REAL time) although we used the CPU for %lf sec (CPU time).\n",
           real_time, cpu_time);

    // Free allocated resources and close any open file descriptors
    for (int i = 0; i < num_builders; i++) {
        free(splitter_to_builder_pipes[i]);
        free(builder_to_root_pipes[i]);
    }

    free(builder_pids);
    free(splitter_pids);
    free(splitter_to_builder_pipes);
    free(builder_to_root_pipes);
    free(word_array);
    free_hash_table(hash_table);
    free(builder_start_times);
    free(builder_end_times);
    free(builder_elapsed_times);

    return 0;
}
