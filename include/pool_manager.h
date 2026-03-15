#ifndef POOL_MANAGER_H
#define POOL_MANAGER_H

#include "header.h"

// Pool management
void init_child_pool(void);
void terminate_child_pool(void);
void reset_pool_slot(int i);
int spawn_child_at(int i);
void batch_spawn_children(int n_to_spawn, int use_omp);
void ensure_children(int need);
int count_alive_children(void);
void check_idle_children(void);

// Task dispatch
void dispatch_collect_elemwise(int opcode, Matrix *A, Matrix *B, Matrix *R, const char *rname);
void dispatch_collect_mul(Matrix *A, Matrix *B, Matrix *R, const char *rname);

// Helper
double now_sec(void);

#endif