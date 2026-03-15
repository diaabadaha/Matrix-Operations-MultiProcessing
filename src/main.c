#include "../include/header.h"
#include "../include/pool_manager.h"
#include <time.h>
#include <sys/select.h>

// External declarations
extern Memory mem;
extern int pipe_menu_to_parent[2];
extern int pipe_parent_to_menu[2];

int main()
{
    // Ignore broken pipe signals
    signal(SIGPIPE, SIG_IGN);
    
    // Initialize global memory
    mem.count = 0;

    // Load matrices from data folder
    fprintf(stderr, "[Main] Loading matrices from ./data/matrices...\n");
    loadMatricesFromFolder("./data/matrices", &mem);

    // Create pipes for menu-parent communication
    create_pipes();

    // Set up signal handlers
    setup_parent_signals();

    // Fork to create menu process
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        // Child process runs the menu
        close(pipe_menu_to_parent[0]);
        close(pipe_parent_to_menu[1]);
        
        run_menu(getppid(), &mem, pipe_menu_to_parent[1], pipe_parent_to_menu[0]);
        exit(0);
    }

    // Parent process
    fprintf(stderr, "[Main] Initializing child pool...\n");
    init_child_pool();
    
    // Close unused pipe ends
    close(pipe_menu_to_parent[1]);
    close(pipe_parent_to_menu[0]);

    fprintf(stderr, "[Main] System ready - waiting for operations...\n\n");
    
    // Wait for signals
    while (1) {
        pause();
    }

    // Cleanup on exit
    terminate_child_pool();
    return 0;
}
