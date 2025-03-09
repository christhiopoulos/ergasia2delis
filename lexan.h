
#include "hash_table.h"
#include <signal.h>

/* Global Variables */
extern volatile sig_atomic_t usr1_count;
extern volatile sig_atomic_t usr2_count;
extern int num_splitters;
extern int num_builders;

/* Signal Handlers */
void handle_usr1(int sig);
void handle_usr2(int sig);




