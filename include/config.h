#ifndef CONFIG_H
#define CONFIG_H

// Maximum limits
#define MAX_MATRICES 100
#define MAX_NAME_LEN 100
#define CHILD_POOL_SIZE 3000

// Matrix structure
typedef struct {
    char name[MAX_NAME_LEN];
    int rows;
    int cols;
    int **data;
} Matrix;

// Memory structure to hold all matrices
typedef struct {
    Matrix matrices[MAX_MATRICES];
    int count;
} Memory;

// Dynamic Child Pool Configuration
#define INITIAL_CHILDREN 100
#define IDLE_TIMEOUT_SEC 60

// Uncomment to enable OpenMP-assisted forking for benchmarking
// #define ALLOW_OMP_FORK 1
// #define OMP_FORK_THRESHOLD 200

#endif