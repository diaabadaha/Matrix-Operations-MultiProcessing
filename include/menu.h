#ifndef MENU_H
#define MENU_H

#include "header.h"

// Main menu loop
void run_menu(pid_t parent_pid, Memory *mem, int write_fd, int read_fd);

#endif