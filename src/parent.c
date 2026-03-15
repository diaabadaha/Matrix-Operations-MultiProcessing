#include "../include/header.h"
#include "../include/pipes.h"
#include "../include/signals.h"
#include "../include/pool_manager.h"
#include <sys/select.h>
#define _XOPEN_SOURCE 500
#include <unistd.h>
#include <sys/time.h>

Memory mem; // Global memory

// Find a matrix by name
static int find_matrix(const char *name, Matrix **out) {
    for (int i = 0; i < mem.count; i++) {
        if (strcmp(mem.matrices[i].name, name) == 0) {
            *out = &mem.matrices[i];
            return 1;
        }
    }
    return 0;
}

// Send result matrix back to menu
static void send_result_matrix_to_menu(const Matrix *m, const char *prefix) {
    char *buf = NULL;
    size_t cap = 4096;
    buf = malloc(cap);
    if (!buf) return;

    if (strncmp(prefix, "RESULT_OK", 9) == 0) {
        int pos = snprintf(buf, cap, "RESULT_OK %s %d %d", m->name, m->rows, m->cols);
        for (int i = 0; i < m->rows; i++) {
            for (int j = 0; j < m->cols; j++) {
                if ((size_t)pos + 16 >= cap) {
                    cap *= 2;
                    buf = realloc(buf, cap);
                }
                pos += snprintf(buf + pos, cap - pos, " %d", m->data[i][j]);
            }
        }
        write(pipe_parent_to_menu[1], buf, strlen(buf) + 1);
    } else {
        write(pipe_parent_to_menu[1], prefix, strlen(prefix) + 1);
    }
    free(buf);
}

// Handle ADD operation
void handle_add(char *args) {
    char Aname[64], Bname[64];
    sscanf(args, "%63s %63s", Aname, Bname);
    
    Matrix *A = NULL, *B = NULL;
    if (!find_matrix(Aname, &A) || !find_matrix(Bname, &B)) {
        write(pipe_parent_to_menu[1], "RESULT_FAIL Matrix_not_found", 29);
        fprintf(stderr, "\n[Operation] ADD failed: Matrix not found\n\n");
        return;
    }
    
    // Check dimensions
    if (A->rows != B->rows || A->cols != B->cols) {
        write(pipe_parent_to_menu[1], "RESULT_FAIL Size_mismatch", 25);
        fprintf(stderr, "\n[Operation] ADD failed: Matrix dimensions don't match\n");
        fprintf(stderr, "            %s is %dx%d, %s is %dx%d\n\n", 
                Aname, A->rows, A->cols, Bname, B->rows, B->cols);
        return;
    }

    Matrix R;
    char rname[MAX_NAME_LEN];
    snprintf(rname, sizeof(rname), "add_%s_%s", Aname, Bname);
    
    fprintf(stderr, "\n[Operation] Starting ADD: %s + %s\n", Aname, Bname);
    double start = omp_get_wtime();
    dispatch_collect_elemwise(10, A, B, &R, rname);
    double end = omp_get_wtime();
    fprintf(stderr, "[Operation] ADD completed in %.6f seconds\n\n", end - start);

    // Store in memory
    if (mem.count < MAX_MATRICES) {
        mem.matrices[mem.count++] = R;
    }

    // Send to menu
    send_result_matrix_to_menu(&R, "RESULT_OK");
}

// Handle SUB operation
void handle_sub(char *args) {
    char Aname[64], Bname[64];
    sscanf(args, "%63s %63s", Aname, Bname);
    
    Matrix *A = NULL, *B = NULL;
    if (!find_matrix(Aname, &A) || !find_matrix(Bname, &B)) {
        write(pipe_parent_to_menu[1], "RESULT_FAIL Matrix_not_found", 29);
        fprintf(stderr, "\n[Operation] SUB failed: Matrix not found\n\n");
        return;
    }
    
    // Check dimensions
    if (A->rows != B->rows || A->cols != B->cols) {
        write(pipe_parent_to_menu[1], "RESULT_FAIL Size_mismatch", 25);
        fprintf(stderr, "\n[Operation] SUB failed: Matrix dimensions don't match\n");
        fprintf(stderr, "            %s is %dx%d, %s is %dx%d\n\n", 
                Aname, A->rows, A->cols, Bname, B->rows, B->cols);
        return;
    }

    Matrix R;
    char rname[MAX_NAME_LEN];
    snprintf(rname, sizeof(rname), "sub_%s_%s", Aname, Bname);
    
    fprintf(stderr, "\n[Operation] Starting SUB: %s - %s\n", Aname, Bname);
    double start = omp_get_wtime();
    dispatch_collect_elemwise(11, A, B, &R, rname);
    double end = omp_get_wtime();
    fprintf(stderr, "[Operation] SUB completed in %.6f seconds\n\n", end - start);

    // Store in memory
    if (mem.count < MAX_MATRICES) {
        mem.matrices[mem.count++] = R;
    }

    // Send to menu
    send_result_matrix_to_menu(&R, "RESULT_OK");
}

// Handle MUL operation
void handle_multiply(char *args) {
    char Aname[64], Bname[64];
    sscanf(args, "%63s %63s", Aname, Bname);
    
    Matrix *A = NULL, *B = NULL;
    if (!find_matrix(Aname, &A) || !find_matrix(Bname, &B)) {
        write(pipe_parent_to_menu[1], "RESULT_FAIL Matrix_not_found", 29);
        fprintf(stderr, "\n[Operation] MUL failed: Matrix not found\n\n");
        return;
    }
    
    if (A->cols != B->rows) {
        write(pipe_parent_to_menu[1], "RESULT_FAIL Bad_dims", 20);
        fprintf(stderr, "\n[Operation] MUL failed: Invalid dimensions for multiplication\n");
        fprintf(stderr, "            %s is %dx%d, %s is %dx%d\n\n", 
                Aname, A->rows, A->cols, Bname, B->rows, B->cols);
        return;
    }

    Matrix R;
    char rname[MAX_NAME_LEN];
    snprintf(rname, sizeof(rname), "mul_%s_%s", Aname, Bname);
    
    fprintf(stderr, "\n[Operation] Starting MUL: %s * %s\n", Aname, Bname);
    double start = omp_get_wtime();
    dispatch_collect_mul(A, B, &R, rname);
    double end = omp_get_wtime();
    fprintf(stderr, "[Operation] MUL completed in %.6f seconds\n\n", end - start);

    // Store in memory
    if (mem.count < MAX_MATRICES) {
        mem.matrices[mem.count++] = R;
    }

    // Send to menu
    send_result_matrix_to_menu(&R, "RESULT_OK");
}

// Handle CREATE
void handle_create(char *args)
{
    char name[64];
    int rows, cols;

    // Parse matrix name, rows, and columns
    sscanf(args, "%s %d %d", name, &rows, &cols);

    // Move pointer to first value
    char *values_ptr = strchr(args, ' ');
    if (values_ptr)
        values_ptr = strchr(values_ptr + 1, ' ');
    if (values_ptr)
        values_ptr = strchr(values_ptr + 1, ' ');
    if (!values_ptr)
    {
        printf("[Parent] Invalid CREATE message format.\n");
        return;
    }

    if (mem.count >= MAX_MATRICES)
    {
        printf("[Parent] Error: memory full.\n");
        return;
    }

    Matrix *m = &mem.matrices[mem.count];
    strcpy(m->name, name);
    m->rows = rows;
    m->cols = cols;

    // Allocate matrix
    m->data = malloc(rows * sizeof(int *));
    for (int i = 0; i < rows; i++)
        m->data[i] = malloc(cols * sizeof(int));

    // Fill data
    char *token = strtok(values_ptr, " \n");
    for (int i = 0; i < rows; i++)
    {
        for (int j = 0; j < cols; j++)
        {
            if (token)
            {
                m->data[i][j] = atoi(token);
                token = strtok(NULL, " \n");
            }
        }
    }

    mem.count++;
    printf("[Parent] Matrix '%s' created (%dx%d).\n", name, rows, cols);
}

// Handle matrix modification operation
void handle_modify(char *args) {
    char name[64], type[16];
    sscanf(args, "%s %s", name, type);

    Matrix *m = NULL;
    for (int i = 0; i < mem.count; i++) {
        if (strcmp(mem.matrices[i].name, name) == 0) {
            m = &mem.matrices[i];
            break;
        }
    }

    if (!m) {
        fprintf(stderr, "\n[Operation] MODIFY failed: Matrix '%s' not found\n\n", name);
        write(pipe_parent_to_menu[1], "MODIFY_FAIL", 12);
        return;
    }

    if (strcmp(type, "VALUE") == 0) {
        // Modify a single value in the matrix
        int r, c, val;
        if (sscanf(args + strlen(name) + strlen(type) + 2, "%d %d %d", &r, &c, &val) != 3) {
            fprintf(stderr, "\n[Operation] MODIFY failed: Invalid format\n\n");
            write(pipe_parent_to_menu[1], "MODIFY_FAIL", 12);
            return;
        }
        
        if (r < 0 || r >= m->rows || c < 0 || c >= m->cols) {
            fprintf(stderr, "\n[Operation] MODIFY failed: Invalid indices\n\n");
            write(pipe_parent_to_menu[1], "MODIFY_FAIL", 12);
            return;
        }
        
        m->data[r][c] = val;
        fprintf(stderr, "\n[Operation] Value modified at (%d,%d) = %d\n\n", r, c, val);
    }
    else if (strcmp(type, "ROW") == 0) {
        // Modify an entire row
        int row;
        if (sscanf(args + strlen(name) + strlen(type) + 2, "%d", &row) != 1) {
            fprintf(stderr, "\n[Operation] MODIFY failed: Invalid format\n\n");
            write(pipe_parent_to_menu[1], "MODIFY_FAIL", 12);
            return;
        }
        
        if (row < 0 || row >= m->rows) {
            fprintf(stderr, "\n[Operation] MODIFY failed: Invalid row\n\n");
            write(pipe_parent_to_menu[1], "MODIFY_FAIL", 12);
            return;
        }

        // Find start of values
        char *ptr = strchr(args, ' ');
        for (int i = 0; i < 2; i++)
            ptr = strchr(ptr + 1, ' ');
        ptr++; // Move past the space
        
        // First pass: Count and validate values
        char *temp_str = strdup(ptr);
        if (!temp_str) {
            fprintf(stderr, "\n[Operation] MODIFY failed: Memory allocation error\n\n");
            write(pipe_parent_to_menu[1], "MODIFY_FAIL", 12);
            return;
        }
        
        char *temp_token = strtok(temp_str, " \t\n");
        int value_count = 0;
        
        while (temp_token != NULL) {
            char *endptr;
            long val = strtol(temp_token, &endptr, 10);
            
            if (*endptr != '\0' || endptr == temp_token) {
                fprintf(stderr, "\n[Operation] MODIFY failed: Invalid value '%s' (only integers allowed)\n\n", temp_token);
                free(temp_str);
                write(pipe_parent_to_menu[1], "MODIFY_FAIL", 12);
                return;
            }
            
            if (val < INT_MIN || val > INT_MAX) {
                fprintf(stderr, "\n[Operation] MODIFY failed: Value out of range\n\n");
                free(temp_str);
                write(pipe_parent_to_menu[1], "MODIFY_FAIL", 12);
                return;
            }
            
            value_count++;
            
            if (value_count > m->cols) {
                fprintf(stderr, "\n[Operation] MODIFY failed: Row is full. Expected %d values but more were provided\n\n", m->cols);
                free(temp_str);
                write(pipe_parent_to_menu[1], "MODIFY_FAIL", 12);
                return;
            }
            
            temp_token = strtok(NULL, " \t\n");
        }
        free(temp_str);
        
        if (value_count != m->cols) {
            fprintf(stderr, "\n[Operation] MODIFY failed: Expected %d values but got %d\n\n", m->cols, value_count);
            write(pipe_parent_to_menu[1], "MODIFY_FAIL", 12);
            return;
        }
        
        // Second pass: Apply modifications (fresh copy)
        temp_str = strdup(ptr);
        if (!temp_str) {
            fprintf(stderr, "\n[Operation] MODIFY failed: Memory allocation error\n\n");
            write(pipe_parent_to_menu[1], "MODIFY_FAIL", 12);
            return;
        }
        
        char *token = strtok(temp_str, " \t\n");
        for (int j = 0; j < m->cols && token; j++) {
            m->data[row][j] = atoi(token);
            token = strtok(NULL, " \t\n");
        }
        free(temp_str);
        
        fprintf(stderr, "\n[Operation] Row %d modified\n\n", row);
    }
    else if (strcmp(type, "COL") == 0) {
        // Modify an entire column
        int col;
        if (sscanf(args + strlen(name) + strlen(type) + 2, "%d", &col) != 1) {
            fprintf(stderr, "\n[Operation] MODIFY failed: Invalid format\n\n");
            write(pipe_parent_to_menu[1], "MODIFY_FAIL", 12);
            return;
        }
        
        if (col < 0 || col >= m->cols) {
            fprintf(stderr, "\n[Operation] MODIFY failed: Invalid column\n\n");
            write(pipe_parent_to_menu[1], "MODIFY_FAIL", 12);
            return;
        }

        // Find start of values
        char *ptr = strchr(args, ' ');
        for (int i = 0; i < 2; i++)
            ptr = strchr(ptr + 1, ' ');
        ptr++; // Move past the space
        
        // First pass: Count and validate values
        char *temp_str = strdup(ptr);
        if (!temp_str) {
            fprintf(stderr, "\n[Operation] MODIFY failed: Memory allocation error\n\n");
            write(pipe_parent_to_menu[1], "MODIFY_FAIL", 12);
            return;
        }
        
        char *temp_token = strtok(temp_str, " \t\n");
        int value_count = 0;
        
        while (temp_token != NULL) {
            char *endptr;
            long val = strtol(temp_token, &endptr, 10);
            
            if (*endptr != '\0' || endptr == temp_token) {
                fprintf(stderr, "\n[Operation] MODIFY failed: Invalid value '%s' (only integers allowed)\n\n", temp_token);
                free(temp_str);
                write(pipe_parent_to_menu[1], "MODIFY_FAIL", 12);
                return;
            }
            
            if (val < INT_MIN || val > INT_MAX) {
                fprintf(stderr, "\n[Operation] MODIFY failed: Value out of range\n\n");
                free(temp_str);
                write(pipe_parent_to_menu[1], "MODIFY_FAIL", 12);
                return;
            }
            
            value_count++;
            
            if (value_count > m->rows) {
                fprintf(stderr, "\n[Operation] MODIFY failed: Column is full. Expected %d values but more were provided\n\n", m->rows);
                free(temp_str);
                write(pipe_parent_to_menu[1], "MODIFY_FAIL", 12);
                return;
            }
            
            temp_token = strtok(NULL, " \t\n");
        }
        free(temp_str);
        
        if (value_count != m->rows) {
            fprintf(stderr, "\n[Operation] MODIFY failed: Expected %d values but got %d\n\n", m->rows, value_count);
            write(pipe_parent_to_menu[1], "MODIFY_FAIL", 12);
            return;
        }
        
        // Second pass: Apply modifications (fresh copy)
        temp_str = strdup(ptr);
        if (!temp_str) {
            fprintf(stderr, "\n[Operation] MODIFY failed: Memory allocation error\n\n");
            write(pipe_parent_to_menu[1], "MODIFY_FAIL", 12);
            return;
        }
        
        char *token = strtok(temp_str, " \t\n");
        for (int i = 0; i < m->rows && token; i++) {
            m->data[i][col] = atoi(token);
            token = strtok(NULL, " \t\n");
        }
        free(temp_str);
        
        fprintf(stderr, "\n[Operation] Column %d modified\n\n", col);
    }

    // Send updated matrix back to menu
    char msg[2048] = "";
    snprintf(msg, sizeof(msg), "MODIFY_OK %s %d %d", m->name, m->rows, m->cols);
    for (int i = 0; i < m->rows; i++)
        for (int j = 0; j < m->cols; j++) {
            char tmp[32];
            sprintf(tmp, " %d", m->data[i][j]);
            strcat(msg, tmp);
        }

    write(pipe_parent_to_menu[1], msg, strlen(msg) + 1);
}

// Handle DELETE
void handle_delete(char *args) {
    char name[64];
    sscanf(args, "%s", name);

    for (int i = 0; i < mem.count; i++) {
        if (strcmp(mem.matrices[i].name, name) == 0) {
            for (int r = 0; r < mem.matrices[i].rows; r++)
                free(mem.matrices[i].data[r]);
            free(mem.matrices[i].data);

            for (int j = i; j < mem.count - 1; j++)
                mem.matrices[j] = mem.matrices[j + 1];
            mem.count--;

            fprintf(stderr, "\n[Operation] Matrix '%s' deleted\n\n", name);
            return;
        }
    }

    fprintf(stderr, "\n[Operation] DELETE failed: Matrix '%s' not found\n\n", name);
}


// Handle DETERMINANT
void handle_determinant(char *args) {
    char name[64];
    sscanf(args, "%63s", name);

    Matrix *m = NULL;
    for (int i = 0; i < mem.count; i++) {
        if (strcmp(mem.matrices[i].name, name) == 0) {
            m = &mem.matrices[i];
            break;
        }
    }

    if (!m) {
        const char *msg = "DET_FAIL Matrix_not_found";
        write(pipe_parent_to_menu[1], msg, strlen(msg) + 1);
        fprintf(stderr, "\n[Operation] DET failed: Matrix not found\n\n");
        return;
    }

    if (m->rows != m->cols) {
        const char *msg = "DET_FAIL Not_square";
        write(pipe_parent_to_menu[1], msg, strlen(msg) + 1);
        fprintf(stderr, "\n[Operation] DET failed: Matrix is not square\n\n");
        return;
    }

    fprintf(stderr, "\n[Operation] Computing determinant for %s (%dx%d)\n", name, m->rows, m->cols);

    PipeChannel ch;
    int state = create_pipe_channel(&ch);
    if (state < 0) {
        const char *msg = "DET_FAIL Channel_error";
        write(pipe_parent_to_menu[1], msg, strlen(msg) + 1);
        fprintf(stderr, "[Operation] DET failed: Channel error\n\n");
        return;
    }

    if (state == 1) {
        // Child side
        extern void determinant_child_work(int read_fd, int write_fd);
        determinant_child_work(ch.to_child[0], ch.to_parent[1]);
        exit(0);
    }

    // Parent side
    TaskHeader hdr = {0};
    hdr.op_code = 13;
    hdr.rowsA = m->rows;
    hdr.colsA = m->cols;
    hdr.rowsB = 0;
    hdr.colsB = 0;

    size_t count = (size_t)m->rows * (size_t)m->cols;
    double *flat = malloc(count * sizeof(double));
    for (int i = 0; i < m->rows; i++)
        for (int j = 0; j < m->cols; j++)
            flat[i * m->cols + j] = (double)m->data[i][j];

    parent_send_task(&ch, &hdr, flat, NULL);
    free(flat);

    ResultHeader rh;
    double result[1];
    if (parent_read_result(&ch, &rh, result) < 0) {
        write(pipe_parent_to_menu[1], "DET_FAIL Read_error", 20);
        fprintf(stderr, "[Operation] DET failed: Read error\n\n");
        destroy_pipe_channel(&ch);
        return;
    }

    char resp[128];
    if (rh.success) {
        snprintf(resp, sizeof(resp), "DET_OK %s %.6f", name, result[0]);
        fprintf(stderr, "[Operation] Determinant = %.6f\n\n", result[0]);
    } else {
        snprintf(resp, sizeof(resp), "DET_FAIL Singular_or_invalid");
        fprintf(stderr, "[Operation] DET failed: Singular matrix\n\n");
    }

    write(pipe_parent_to_menu[1], resp, strlen(resp) + 1);
    destroy_pipe_channel(&ch);
}

// Handle EIGENVALUES
void handle_eigenvalues(char *args)
{
    char name[64];
    sscanf(args, "%63s", name);

    Matrix *m = NULL;
    for (int i = 0; i < mem.count; i++)
    {
        if (strcmp(mem.matrices[i].name, name) == 0)
        {
            m = &mem.matrices[i];
            break;
        }
    }

    if (!m)
    {
        const char *msg = "EIGEN_FAIL Matrix_not_found";
        write(pipe_parent_to_menu[1], msg, strlen(msg) + 1);
        return;
    }

    if (m->rows != m->cols)
    {
        const char *msg = "EIGEN_FAIL Not_square";
        write(pipe_parent_to_menu[1], msg, strlen(msg) + 1);
        return;
    }

    int n = m->rows;

    printf("\n[Parent] Computing eigenvalues for matrix '%s' (%dx%d)...\n", name, n, n);
    printf("[Parent] Using QR iteration with OpenMP parallelization\n");
    printf("[Parent] Matrix is %s\n\n", is_symmetric(m) ? "SYMMETRIC" : "NON-SYMMETRIC");

    double total_start = omp_get_wtime();

    // Flatten matrix
    size_t count = (size_t)n * n;
    double *A_work = malloc(count * sizeof(double));
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            A_work[i * n + j] = (double)m->data[i][j];

    // QR Iteration
    int max_iter = 1000;
    double tolerance = 1e-9;

    int max_threads = omp_get_max_threads();
    if (n < 10) omp_set_num_threads(2);
    else if (n < 50) omp_set_num_threads(max_threads / 2);
    else omp_set_num_threads(max_threads);

    for (int iter = 0; iter < max_iter; iter++)
    {
        for (int j = 0; j < n - 1; j++)
        {
            for (int i = n - 1; i > j; i--)
            {
                double a = A_work[(i - 1) * n + j];
                double b = A_work[i * n + j];
                if (fabs(b) < 1e-12) continue;

                double r = sqrt(a * a + b * b);
                double c = a / r;
                double s = -b / r;

                #pragma omp parallel for
                for (int k = 0; k < n; k++)
                {
                    double t1 = A_work[(i - 1) * n + k];
                    double t2 = A_work[i * n + k];
                    A_work[(i - 1) * n + k] = c * t1 - s * t2;
                    A_work[i * n + k] = s * t1 + c * t2;
                }

                #pragma omp parallel for
                for (int k = 0; k < n; k++)
                {
                    double t1 = A_work[k * n + (i - 1)];
                    double t2 = A_work[k * n + i];
                    A_work[k * n + (i - 1)] = c * t1 - s * t2;
                    A_work[k * n + i] = s * t1 + c * t2;
                }
            }
        }

        double off_diag = 0.0;
        for (int i = 0; i < n; i++)
            for (int j = 0; j < n; j++)
                if (i != j) off_diag += fabs(A_work[i * n + j]);

        if (off_diag < tolerance)
        {
            printf("[Parent] QR iteration converged in %d iterations\n", iter + 1);
            break;
        }
    }

    double total_end = omp_get_wtime();
    double multi_process_time = total_end - total_start;

    // Eigenvalue extraction
    typedef struct
    {
        ComplexNumber eigenvalue;
        double *eigenvector;
        int success;
    } EigenPair;

    EigenPair *results = malloc(n * sizeof(EigenPair));
    int num_found = 0;

    int i = 0;
    while (i < n)
    {
        if (i < n - 1 && fabs(A_work[(i + 1) * n + i]) > 1e-6)
        {
            double a = A_work[i * n + i];
            double b = A_work[i * n + (i + 1)];
            double c = A_work[(i + 1) * n + i];
            double d = A_work[(i + 1) * n + (i + 1)];

            double trace = a + d;
            double det = a * d - b * c;
            double disc = trace * trace / 4.0 - det;

            if (disc < 0)
            {
                double real_part = trace / 2.0;
                double imag_part = sqrt(-disc);

                results[num_found].eigenvalue.real = real_part;
                results[num_found].eigenvalue.imag = imag_part;
                results[num_found].eigenvector = NULL;
                results[num_found].success = 1;
                num_found++;

                results[num_found].eigenvalue.real = real_part;
                results[num_found].eigenvalue.imag = -imag_part;
                results[num_found].eigenvector = NULL;
                results[num_found].success = 1;
                num_found++;

                i += 2;
            }
            else
            {
                results[num_found].eigenvalue.real = A_work[i * n + i];
                results[num_found].eigenvalue.imag = 0.0;
                results[num_found].eigenvector = NULL;
                results[num_found].success = 1;
                num_found++;
                i++;
            }
        }
        else
        {
            results[num_found].eigenvalue.real = A_work[i * n + i];
            results[num_found].eigenvalue.imag = 0.0;
            results[num_found].eigenvector = NULL;
            results[num_found].success = 1;
            num_found++;
            i++;
        }
    }

    // Eigenvector computation via Inverse Iteration
    for (int k = 0; k < num_found; k++)
    {
        results[k].eigenvector = malloc(n * sizeof(double));

        if (fabs(results[k].eigenvalue.imag) > 1e-8)
        {
            for (int j = 0; j < n; j++)
                results[k].eigenvector[j] = 0.0;
            continue;
        }

        double lambda = results[k].eigenvalue.real;
        double *A_shift = malloc(n * n * sizeof(double));

        double epsilon = 1e-6;  // small stabilizer
        for (int i = 0; i < n; i++)
            for (int j = 0; j < n; j++)
                A_shift[i * n + j] = (i == j)
                    ? (m->data[i][j] - lambda + epsilon)
                    : (double)m->data[i][j];

        EigenResult ev = inverse_iteration_omp(A_shift, n, 500, 1e-9);

        if (ev.converged)
            memcpy(results[k].eigenvector, ev.eigenvector, n * sizeof(double));
        else
            for (int j = 0; j < n; j++) results[k].eigenvector[j] = 0.0;

        free_eigen_result(&ev);
        free(A_shift);
    }

    // Single-threaded comparison
    printf("[Parent] Running single-threaded comparison...\n");

    double single_start = omp_get_wtime();
    omp_set_num_threads(1);

    double *A_single = malloc(count * sizeof(double));
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            A_single[i * n + j] = (double)m->data[i][j];

    for (int iter = 0; iter < 100; iter++)
    {
        for (int j = 0; j < n - 1; j++)
        {
            for (int i = n - 1; i > j; i--)
            {
                double a = A_single[(i - 1) * n + j];
                double b = A_single[i * n + j];
                if (fabs(b) < 1e-12) continue;

                double r = sqrt(a * a + b * b);
                double c = a / r;
                double s = -b / r;

                for (int k = 0; k < n; k++)
                {
                    double t1 = A_single[(i - 1) * n + k];
                    double t2 = A_single[i * n + k];
                    A_single[(i - 1) * n + k] = c * t1 - s * t2;
                    A_single[i * n + k] = s * t1 + c * t2;
                }

                for (int k = 0; k < n; k++)
                {
                    double t1 = A_single[k * n + (i - 1)];
                    double t2 = A_single[k * n + i];
                    A_single[k * n + (i - 1)] = c * t1 - s * t2;
                    A_single[k * n + i] = s * t1 + c * t2;
                }
            }
        }
    }

    double single_end = omp_get_wtime();
    double single_thread_time = single_end - single_start;
    free(A_single);
    omp_set_num_threads(max_threads);

    // Display results
    printf("\nEigenvalues of %s:\n", name);
    int success_count = 0;
    for (int k = 0; k < num_found; k++)
    {
        if (!results[k].success) continue;
        success_count++;

        if (fabs(results[k].eigenvalue.imag) < 1e-10)
            printf("λ%d = %.6f\n", k + 1, results[k].eigenvalue.real);
        else
            printf("λ%d = %.6f %s %.6fi\n",
                   k + 1,
                   results[k].eigenvalue.real,
                   results[k].eigenvalue.imag >= 0 ? "+" : "",
                   results[k].eigenvalue.imag);
    }

    printf("\nEigenvectors:\n");
    for (int k = 0; k < num_found; k++)
    {
        if (!results[k].success || !results[k].eigenvector) continue;
        printf("v%d = [", k + 1);
        for (int j = 0; j < n; j++)
        {
            printf("%.3f", results[k].eigenvector[j]);
            if (j < n - 1) printf(" ");
        }
        printf("]\n");
    }

    printf("\n═══════════════════════════════════════════════════════════════\n");
    printf("PERFORMANCE COMPARISON:\n");
    printf("  Multi-threaded (OpenMP):  %.6f seconds\n", multi_process_time);
    printf("  Single-threaded:          %.6f seconds\n", single_thread_time);
    printf("  Speedup:                  %.2fx\n", single_thread_time / multi_process_time);
    printf("  Eigenvalues found:        %d / %d\n", success_count, n);
    printf("═══════════════════════════════════════════════════════════════\n\n");

    // Send results to menu
    char resp[2048];
    int pos = snprintf(resp, sizeof(resp), "EIGEN_OK %d", num_found);
    for (int k = 0; k < num_found; k++)
    {
        if (results[k].success)
            pos += snprintf(resp + pos, sizeof(resp) - pos, " %.10f %.10f",
                            results[k].eigenvalue.real,
                            results[k].eigenvalue.imag);
    }
    write(pipe_parent_to_menu[1], resp, strlen(resp) + 1);

    // Cleanup
    for (int i = 0; i < num_found; i++)
        if (results[i].eigenvector) free(results[i].eigenvector);
    free(results);
    free(A_work);
}