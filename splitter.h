

#define INITIAL_PIPE_CAPACITY 10
#define MAX_WORD_LENGTH 100


typedef struct ExclusionWord {
    char *word;
    struct ExclusionWord *left;
    struct ExclusionWord *right;
} ExclusionWord;

typedef struct ExclusionTree {
    ExclusionWord *root;
} ExclusionTree;


ExclusionWord* create_exclusion_word(const char *word);
void insert_exclusion_word(ExclusionTree *tree, const char *word);
int is_excluded(ExclusionTree *tree, const char *word);
void free_exclusion_tree(ExclusionWord *node);


void strip_punctuation(char *word);


