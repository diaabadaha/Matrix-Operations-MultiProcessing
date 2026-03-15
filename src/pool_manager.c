#include "../include/header.h"
#include "../include/pipes.h"
#include "../include/config.h"

static PipeChannel pool[CHILD_POOL_SIZE];
static int  pool_busy[CHILD_POOL_SIZE]       = {0};
static int  pool_alive[CHILD_POOL_SIZE]      = {0};
static double pool_last_used[CHILD_POOL_SIZE] = {0.0};
static int  pool_count = 0; // number of current alive children

void reset_pool_slot(int i) {
    memset(&pool[i], 0, sizeof(PipeChannel));
    pool_busy[i]  = 0;
    pool_alive[i] = 0;
    pool_last_used[i] = 0.0;
}


// Return current timestamp in seconds
static inline double now_sec(void) { return omp_get_wtime(); }

// Create one child in slot i
int spawn_child_at(int i) {
    int state = create_pipe_channel(&pool[i]);
    if (state < 0) {
        perror("create_pipe_channel");
        return -1;
    }
    if (state == 1) {
        child_loop(pool[i].to_child[0], pool[i].to_parent[1]);
        _exit(0);
    }
    pool_busy[i] = 0;
    pool_alive[i] = 1;
    pool_last_used[i] = now_sec();
    return 0;
}

// Spawn multiple children, optionally with OpenMP
void batch_spawn_children(int n_to_spawn, int use_omp) {
    if (n_to_spawn <= 0) return;
    if (pool_count + n_to_spawn > CHILD_POOL_SIZE)
        n_to_spawn = CHILD_POOL_SIZE - pool_count;

    double t0 = now_sec();

#ifdef ALLOW_OMP_FORK
    if (use_omp) {
        #pragma omp parallel for schedule(static)
        for (int k = 0; k < n_to_spawn; k++)
            spawn_child_at(pool_count + k);
    } else
#endif
    {
        for (int k = 0; k < n_to_spawn; k++)
            spawn_child_at(pool_count + k);
    }

    pool_count += n_to_spawn;
    double t1 = now_sec();
    fprintf(stderr, "[Parent] Spawned %d child(ren) in %.6f sec (%s)\n",
           n_to_spawn, t1 - t0,
#ifdef ALLOW_OMP_FORK
           use_omp ? "OpenMP" :
#endif
           "serial");
    fflush(stderr);
}

// Ensure we have at least 'need' total alive children
int count_alive_children(void) {
    int alive = 0;
    for (int i = 0; i < pool_count; i++)
        if (pool_alive[i]) alive++;
    return alive;
}

void ensure_children(int need) {
    if (need > CHILD_POOL_SIZE) need = CHILD_POOL_SIZE;

    int alive = count_alive_children();

    // if no children alive at all, restart pool cleanly
    if (alive == 0) {
        fprintf(stderr, "[Parent] No alive children, restarting pool...\n");
        for (int i = 0; i < pool_count; i++)
            reset_pool_slot(i);
        pool_count = 0;
        int spawn_n = (need < INITIAL_CHILDREN ? INITIAL_CHILDREN : need);
        batch_spawn_children(spawn_n, 0);
        return;
    }

    // remove any half-dead slots (alive flag inconsistent)
    for (int i = 0; i < pool_count; i++) {
        if (!pool_alive[i] && (pool[i].to_child[0] != 0 || pool[i].to_parent[1] != 0))
            reset_pool_slot(i);
    }

    int to_spawn = need - alive;
    if (to_spawn <= 0) return;

    int use_omp = 0;
#ifdef ALLOW_OMP_FORK
    if (to_spawn >= OMP_FORK_THRESHOLD) use_omp = 1;
#endif
    batch_spawn_children(to_spawn, use_omp);
}


// Kill idle children older than IDLE_TIMEOUT_SEC
void check_idle_children(void) {
    double now = now_sec();
    for (int i = 0; i < pool_count; i++) {
        if (!pool_alive[i] || pool_busy[i]) continue;
        if (now - pool_last_used[i] > IDLE_TIMEOUT_SEC) {
            TaskHeader hdr = { .op_code = 255 };

            // Try to send the shutdown signal to the child
            if (parent_send_task(&pool[i], &hdr, (double[]){0}, NULL) < 0) {
                // If the write fails (pipe broken), handle it safely
                fprintf(stderr, "[Parent] Warning: failed to send shutdown to child %d (pipe dead)\n", i);
                reset_pool_slot(i);
                pool_alive[i] = 0;
                pool_busy[i] = 0;
                continue; // skip to next child
            }

            destroy_pipe_channel(&pool[i]);
            pool_alive[i] = 0;
            pool_busy[i] = 0;
            reset_pool_slot(i);
            fprintf(stderr, "[Parent] Killed idle child %d\n", i);
        }

    }
}

void init_child_pool(void) {
    int first_batch = (INITIAL_CHILDREN > CHILD_POOL_SIZE) ? CHILD_POOL_SIZE : INITIAL_CHILDREN;
    batch_spawn_children(first_batch, /*use_omp=*/0);
    fprintf(stderr, "[Parent] Child pool initialized with %d/%d children (dynamic)\n",
           pool_count, CHILD_POOL_SIZE);
}
void terminate_child_pool(void) {
    TaskHeader hdr = { .op_code = 255 };
    for (int i = 0; i < pool_count; i++) {
        if (!pool_alive[i]) continue;
        parent_send_task(&pool[i], &hdr, (double[]){0}, NULL);
        destroy_pipe_channel(&pool[i]);
        pool_alive[i] = 0;
        pool_busy[i] = 0;
    }
    fprintf(stderr, "[Parent] Child pool terminated.\n");
}

void dispatch_collect_elemwise(
    int opcode, Matrix *A, Matrix *B, Matrix *R, const char *rname)
{
    snprintf(R->name, MAX_NAME_LEN, "%s", rname);
    R->rows = A->rows; R->cols = A->cols;
    R->data = malloc(R->rows * sizeof(int*));
    for (int i = 0; i < R->rows; i++) R->data[i] = malloc(R->cols * sizeof(int));

    int total = A->rows * A->cols, next = 0, done = 0;
    while (done < total) {
        // dispatch
        for (int j = 0; j < pool_count && next < total; j++) {
            if (!pool_busy[j]) {
                int r = next / A->cols, c = next % A->cols;
                TaskHeader hdr = {
                    .op_code = opcode,
                    .rowsA = 1, .colsA = 1,
                    .rowsB = 1, .colsB = 1,
                    .r = r, .c = c
                };

                double a = A->data[r][c], b = B->data[r][c];
                if (parent_send_task(&pool[j], &hdr, &a, &b) < 0) continue;
                pool_busy[j] = 1;
                pool[j].op_row = r; pool[j].op_col = c;
                next++;
            }
        }
        // collect
        for (int j = 0; j < pool_count; j++) {
            if (!pool_busy[j]) continue;

            ResultHeader rh; double val;
            int ret = parent_read_result(&pool[j], &rh, &val);
            if (ret == 0 && rh.success) {
                if (rh.r >= 0 && rh.r < R->rows && rh.c >= 0 && rh.c < R->cols) {
                    R->data[rh.r][rh.c] = (int)val;
                }
                pool_busy[j] = 0;
                pool_last_used[j] = now_sec();
                done++;
            }

        }

        usleep(1000);
    }
}

void dispatch_collect_mul(Matrix *A, Matrix *B, Matrix *R, const char *rname)
{
    snprintf(R->name, MAX_NAME_LEN, "%s", rname);
    R->rows = A->rows;
    R->cols = B->cols;
    R->data = malloc(R->rows * sizeof(int *));
    for (int i = 0; i < R->rows; i++)
        R->data[i] = malloc(R->cols * sizeof(int));

    int total = A->rows * B->cols;
    int next = 0, done = 0;

    while (done < total) {
        // Dispatch phase
        for (int j = 0; j < pool_count && next < total; j++) {
            if (!pool_busy[j]) {
                int r = next / B->cols;
                int c = next % B->cols;

                // prepare the row of A and column of B
                double *row = malloc(A->cols * sizeof(double));
                double *col = malloc(A->cols * sizeof(double));  // shared dimension = A->cols
                for (int k = 0; k < A->cols; k++) {
                    row[k] = A->data[r][k];
                    col[k] = B->data[k][c];
                }

                TaskHeader hdr = {
                    .op_code = 12,
                    .rowsA = 1,
                    .colsA = A->cols,   // shared dimension
                    .rowsB = A->cols,   // must match colsA
                    .colsB = 1,
                    .r = r,
                    .c = c
                };

                if (parent_send_task(&pool[j], &hdr, row, col) == 0) {
                    pool_busy[j] = 1;
                    pool[j].op_row = r;
                    pool[j].op_col = c;
                    next++;
                }

                free(row);
                free(col);
            }
        }

        // Collect phase
        for (int j = 0; j < CHILD_POOL_SIZE; j++) {
            if (!pool_busy[j]) continue;

            ResultHeader rh;
            double val;
            int ret = parent_read_result(&pool[j], &rh, &val);
            if (ret == 0 && rh.success) {
                if (rh.r >= 0 && rh.r < R->rows && rh.c >= 0 && rh.c < R->cols) {
                    R->data[rh.r][rh.c] = (int)val;
                }
                pool_busy[j] = 0;
                pool_last_used[j] = now_sec();
                done++;
            }

        }

        usleep(1000);
    }
}
