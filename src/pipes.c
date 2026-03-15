#include "../include/header.h"

// Global pipes for menu-parent communication
int pipe_menu_to_parent[2];
int pipe_parent_to_menu[2];

// Create pipes for menu-parent communication
void create_pipes(void) {
    if (pipe(pipe_menu_to_parent) == -1) {
        perror("pipe_menu_to_parent");
        exit(1);
    }
    if (pipe(pipe_parent_to_menu) == -1) {
        perror("pipe_parent_to_menu");
        exit(1);
    }
}

// Write full buffer to file descriptor
ssize_t write_full(int fd, const void *buf, size_t n) {
    size_t off = 0;
    const char *p = buf;
    
    while (off < n) {
        ssize_t w = write(fd, p + off, n - off);
        if (w < 0) {
            if (errno == EINTR) continue;
            if (errno == EPIPE) {
                fprintf(stderr, "[Pipe] Broken pipe detected\n");
            }
            return -1;
        }
        off += (size_t)w;
    }
    return (ssize_t)off;
}

// Read full buffer from file descriptor
ssize_t read_full(int fd, void *buf, size_t n) {
    size_t off = 0;
    char *p = buf;
    
    while (off < n) {
        ssize_t r = read(fd, p + off, n - off);
        if (r == 0) break;
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        off += (size_t)r;
    }
    return (ssize_t)off;
}

// Create a two-way pipe channel between parent and child
int create_pipe_channel(PipeChannel *ch) {
    if (pipe(ch->to_child) < 0) return -1;
    if (pipe(ch->to_parent) < 0) {
        close(ch->to_child[0]);
        close(ch->to_child[1]);
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) return -1;

    if (pid == 0) {
        // Child side
        close(ch->to_child[1]);
        close(ch->to_parent[0]);
        ch->pid = 0;
        return 1;
    } else {
        // Parent side
        close(ch->to_child[0]);
        close(ch->to_parent[1]);
        ch->pid = pid;
        return 0;
    }
}

// Destroy a pipe channel
void destroy_pipe_channel(PipeChannel *ch) {
    close(ch->to_child[0]);
    close(ch->to_child[1]);
    close(ch->to_parent[0]);
    close(ch->to_parent[1]);
}

// Parent sends task to child
int parent_send_task(PipeChannel *ch, const TaskHeader *hdr, const double *A_slice, const double *B_slice) {
    // Send header
    if (write_full(ch->to_child[1], hdr, sizeof(*hdr)) != sizeof(*hdr))
        return -1;

    // Send A data
    size_t countA = (size_t)hdr->rowsA * (size_t)hdr->colsA;
    if (write_full(ch->to_child[1], A_slice, countA * sizeof(double)) != (ssize_t)(countA * sizeof(double)))
        return -1;

    // Send B data if needed
    if (hdr->rowsB > 0 && B_slice) {
        size_t countB = (size_t)hdr->rowsB * (size_t)hdr->colsB;
        if (write_full(ch->to_child[1], B_slice, countB * sizeof(double)) != (ssize_t)(countB * sizeof(double)))
            return -1;
    }

    return 0;
}

// Child reads task from parent
int child_read_task(int fd, TaskHeader *hdr, double **A_slice, double **B_slice) {
    if (read_full(fd, hdr, sizeof(*hdr)) != sizeof(*hdr))
        return -1;

    // Read A data
    size_t countA = (size_t)hdr->rowsA * (size_t)hdr->colsA;
    *A_slice = malloc(countA * sizeof(double));
    if (!*A_slice) return -1;
    if (read_full(fd, *A_slice, countA * sizeof(double)) != (ssize_t)(countA * sizeof(double)))
        return -1;

    // Read B data if needed
    if (hdr->rowsB > 0) {
        size_t countB = (size_t)hdr->rowsB * (size_t)hdr->colsB;
        *B_slice = malloc(countB * sizeof(double));
        if (!*B_slice) return -1;
        if (read_full(fd, *B_slice, countB * sizeof(double)) != (ssize_t)(countB * sizeof(double)))
            return -1;
    } else {
        *B_slice = NULL;
    }

    return 0;
}

// Child sends result to parent
int child_send_result(int fd, const ResultHeader *hdr, const double *result) {
    if (write_full(fd, hdr, sizeof(*hdr)) != sizeof(*hdr))
        return -1;

    if (hdr->success) {
        size_t count = (size_t)hdr->rows * (size_t)hdr->cols;
        if (write_full(fd, result, count * sizeof(double)) != (ssize_t)(count * sizeof(double)))
            return -1;
    }

    return 0;
}

// Parent reads result from child
int parent_read_result(PipeChannel *ch, ResultHeader *hdr, double *buffer) {
    if (read_full(ch->to_parent[0], hdr, sizeof(*hdr)) != sizeof(*hdr))
        return -1;

    if (hdr->success) {
        size_t count = (size_t)hdr->rows * (size_t)hdr->cols;
        if (read_full(ch->to_parent[0], buffer, count * sizeof(double)) != (ssize_t)(count * sizeof(double)))
            return -1;
    }
    return 0;
}
