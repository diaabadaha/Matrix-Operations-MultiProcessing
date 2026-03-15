#define _POSIX_C_SOURCE 200809L
#include "../include/header.h"

extern Memory mem;
extern int pipe_menu_to_parent[2];
extern int pipe_parent_to_menu[2];

// Handle SIGUSR1 - message from menu
void handle_sigusr1(int sig) {
    (void)sig;  // Unused parameter
    
    char buffer[1024];
    ssize_t n = read(pipe_menu_to_parent[0], buffer, sizeof(buffer) - 1);
    if (n <= 0) {
        perror("read from menu pipe");
        return;
    }
    buffer[n] = '\0';

    // Dispatch to appropriate handler
    if (strncmp(buffer, "CREATE", 6) == 0) {
        handle_create(buffer + 7);
        write(pipe_parent_to_menu[1], "CREATE_OK", 10);
    } 
    else if (strncmp(buffer, "DELETE", 6) == 0) {
        handle_delete(buffer + 7);
        write(pipe_parent_to_menu[1], "DELETE_OK", 10);
    } 
    else if (strncmp(buffer, "MODIFY", 6) == 0) {
        handle_modify(buffer + 7);
    }
    else if (strncmp(buffer, "ADD", 3) == 0) {
        handle_add(buffer + 4);
    }
    else if (strncmp(buffer, "SUB", 3) == 0) {
        handle_sub(buffer + 4);
    }
    else if (strncmp(buffer, "MUL", 3) == 0) {
        handle_multiply(buffer + 4);
    }
    else if (strncmp(buffer, "DET", 3) == 0) {
        handle_determinant(buffer + 4);
    }
    else if (strncmp(buffer, "EIGEN", 5) == 0) {
        handle_eigenvalues(buffer + 6);
    }
}

// Handle SIGUSR2 - exit signal
void handle_sigusr2(int sig) {
    (void)sig;  // Unused parameter
    
    printf("\n[System] Shutting down...\n");
    exit(0);
}

// Set up signal handlers
void setup_parent_signals(void) {
    struct sigaction sa1, sa2;

    sa1.sa_handler = handle_sigusr1;
    sigemptyset(&sa1.sa_mask);
    sa1.sa_flags = 0;
    sigaction(SIGUSR1, &sa1, NULL);

    sa2.sa_handler = handle_sigusr2;
    sigemptyset(&sa2.sa_mask);
    sa2.sa_flags = 0;
    sigaction(SIGUSR2, &sa2, NULL);
}