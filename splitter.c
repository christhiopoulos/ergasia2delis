#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <ctype.h>
#include"splitter.h"



#define INITIAL_PIPE_CAPACITY 10
#define MAX_WORD_LENGTH 100



// Συνάρτηση για δημιουργία νέου κόμβου exclusion word
ExclusionWord* create_exclusion_word(const char *word) {
    ExclusionWord *node = malloc(sizeof(ExclusionWord));
    if (!node) {
        perror("malloc");
        exit(1);
    }
    node->word = strdup(word);
    if (!node->word) {
        perror("strdup");
        exit(1);
    }
    node->left = node->right = NULL;
    return node;
}

// Συνάρτηση για εισαγωγή λέξης στο exclusion tree
void insert_exclusion_word(ExclusionTree *tree, const char *word) {
    ExclusionWord **current = &(tree->root);
    while (*current) {
        int cmp = strcmp(word, (*current)->word);
        if (cmp < 0) {
            current = &((*current)->left);
        } else if (cmp > 0) {
            current = &((*current)->right);
        } else {
            // Η λέξη υπάρχει ήδη στο δέντρο
            return;
        }
    }
    *current = create_exclusion_word(word);
}

// Συνάρτηση για έλεγχο αν μια λέξη είναι εξαιρούμενη
int is_excluded(ExclusionTree *tree, const char *word) {
    ExclusionWord *current = tree->root;
    while (current) {
        int cmp = strcmp(word, current->word);
        if (cmp < 0) {
            current = current->left;
        } else if (cmp > 0) {
            current = current->right;
        } else {
            return 1;
        }
    }
    return 0;
}

// Συνάρτηση για ελευθέρωση του exclusion tree
void free_exclusion_tree(ExclusionWord *node) {
    if (node) {
        free_exclusion_tree(node->left);
        free_exclusion_tree(node->right);
        free(node->word);
        free(node);
    }
}


// Συνάρτηση για αφαίρεση σημείων στίξης από μια λέξη
void strip_punctuation(char *word) {
    char *src = word, *dst = word;
    while (*src) {
        // Copy only non-punctuation characters and skip double quotes
        if (!ispunct((unsigned char)*src)) {
            *dst++ = *src;
        }
        src++;
    }
    *dst = '\0';
}
int main(int argc, char *argv[]) {
    if (argc < 6) {
        fprintf(stderr, "Usage: %s <splitter_id> <input_file> <exclusion_file> <num_builders> <pipe_fds>\n", argv[0]);
        return 1;
    }

    // Μετατροπή των παραμέτρων
    int num_builders = atoi(argv[4]);
    char *input_file = argv[2];
    char *exclusion_file = argv[3];
    char *pipe_fds_str = argv[5];

    // Δυναμική διάθεση μνήμης για τους file descriptors
    int fd_capacity = INITIAL_PIPE_CAPACITY;
    int fd_count = 0;
    int *pipe_fds = malloc(fd_capacity * sizeof(int));
    if (!pipe_fds) {
        perror("malloc");
        return 1;
    }

    // Διαχωρισμός των pipe file descriptors από τη συμβολοσειρά
    char *token = strtok(pipe_fds_str, " ");
    while (token != NULL) {
        if (fd_count >= fd_capacity) {
            // Διπλασιασμός του χώρου όταν φτάσουμε στο όριο
            fd_capacity *= 2;
            int *temp = realloc(pipe_fds, fd_capacity * sizeof(int));
            if (!temp) {
                perror("realloc");
                free(pipe_fds);
                return 1;
            }
            pipe_fds = temp;
        }
        pipe_fds[fd_count++] = atoi(token);
        token = strtok(NULL, " ");
    }

    // Δημιουργία του exclusion tree
    ExclusionTree exclusion_tree = { .root = NULL };

    FILE *excl_fp = fopen(exclusion_file, "r");
    if (!excl_fp) {
        perror("fopen exclusion_file");
        free(pipe_fds);
        return 1;
    }

    char excl_word[MAX_WORD_LENGTH];
    while (fscanf(excl_fp, "%s", excl_word) != EOF) {
        insert_exclusion_word(&exclusion_tree, excl_word);
    }
    fclose(excl_fp);

    FILE *in_fp = fopen(input_file, "r");
    if (!in_fp) {
        perror("fopen input_file");
        free_exclusion_tree(exclusion_tree.root);
        free(pipe_fds);
        return 1;;
    }

    int total_words_sent = 0;
    char *line = NULL;
     ssize_t read;
      size_t len = 0;
    while ((read = getline(&line, &len, in_fp)) != -1) {
        char *word = strtok(line, " \t\n");
        while (word != NULL) {


 
            // Remove punctuation
            strip_punctuation(word);

            // Convert to lowercase
            for (char *p = word; *p; ++p) {
                *p = tolower((unsigned char)*p);
            }

            // Skip empty or excluded words
            if (strlen(word) == 0 || is_excluded(&exclusion_tree, word)) {
                word = strtok(NULL, " \t\n");
                continue;
            }

            int builder_index = total_words_sent % num_builders;
            if (builder_index >= fd_count) {
                fprintf(stderr, "Error: Not enough pipe file descriptors provided.\n");
                free(line);
                fclose(in_fp);
                free_exclusion_tree(exclusion_tree.root);
                free(pipe_fds);
                return 1;
            }

            int write_fd = pipe_fds[builder_index];
            if (dprintf(write_fd, "%s\n", word) < 0) {
                perror("write word to builder pipe");
                free(line);
                fclose(in_fp);
                free_exclusion_tree(exclusion_tree.root);
                free(pipe_fds);
                return 1;
            }

            total_words_sent++;
            word = strtok(NULL, " \t\n");
        }
    }
    free(line);
    fclose(in_fp);

    // Κλείσιμο των write ends των pipes μετά την αποστολή όλων των λέξεων
    for (int i = 0; i < fd_count; i++) {
        close(pipe_fds[i]);
    }

    // Αποστολή σήματος SIGUSR1 στον γονέα για να ενημερωθεί ότι ολοκληρώθηκε η αποστολή λέξεων
    if (kill(getppid(), SIGUSR1) == -1) {
        perror("kill SIGUSR1");
        free_exclusion_tree(exclusion_tree.root);
        free(pipe_fds);
        return 1;
    }

    // Απελευθέρωση του exclusion tree και της μνήμης των pipe file descriptors
    free_exclusion_tree(exclusion_tree.root);
    free(pipe_fds);

    return 0;
}
