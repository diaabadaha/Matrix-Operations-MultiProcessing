#ifndef SIGNALS_H
#define SIGNALS_H

// Signal setup
void setup_parent_signals(void);

// Signal handlers
void handle_sigusr1(int sig);
void handle_sigusr2(int sig);

// Message handlers
void handle_create(char *args);
void handle_delete(char *args);
void handle_modify(char *args);
void handle_add(char *args);
void handle_sub(char *args);
void handle_multiply(char *args);
void handle_determinant(char *args);
void handle_eigenvalues(char *args);

#endif