#ifndef PIPES_H
#define PIPES_H

#include "header.h"

// Global pipes for menu-parent communication
extern int pipe_menu_to_parent[2];
extern int pipe_parent_to_menu[2];

// Two-way pipe for parent-child communication
typedef struct {
    int to_child[2];
    int to_parent[2];
    pid_t pid;
    int op_row;
    int op_col;
} PipeChannel;

// Task header sent from parent to child
typedef struct __attribute__((packed)) {
    int op_code;
    int rowsA;
    int colsA;
    int rowsB;
    int colsB;
    int r;
    int c;
} TaskHeader;

// Result header sent from child to parent
typedef struct __attribute__((packed)) {
    int rows;
    int cols;
    int success;
    int r;
    int c;
} ResultHeader;

// Pipe creation and management
void create_pipes(void);
int create_pipe_channel(PipeChannel *ch);
void destroy_pipe_channel(PipeChannel *ch);

// IO helpers
ssize_t write_full(int fd, const void *buf, size_t n);
ssize_t read_full(int fd, void *buf, size_t n);

// Communication functions
int parent_send_task(PipeChannel *ch, const TaskHeader *hdr, const double *A_slice, const double *B_slice);
int child_read_task(int fd, TaskHeader *hdr, double **A_slice, double **B_slice);
int child_send_result(int fd, const ResultHeader *hdr, const double *result);
int parent_read_result(PipeChannel *ch, ResultHeader *hdr, double *buffer);

// Child loop
void child_loop(int read_fd, int write_fd);

#endif