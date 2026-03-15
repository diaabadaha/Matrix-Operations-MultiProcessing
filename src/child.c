#include "../include/header.h"

// Child process work for determinant calculation
void determinant_child_work(int read_fd, int write_fd) {
    // Read task header from parent (contains matrix size info)
    TaskHeader hdr;
    if (read_full(read_fd, &hdr, sizeof(hdr)) != sizeof(hdr))
        exit(1);

    // Prepare result header for response
    ResultHeader rh = {0};

    // Check if matrix is square; determinant is defined only for square matrices
    if (hdr.rowsA != hdr.colsA) {
        rh.success = 0;
        rh.rows = hdr.rowsA;
        rh.cols = hdr.colsA;
        child_send_result(write_fd, &rh, NULL);
        exit(0);
    }

    int n = hdr.rowsA;
    size_t count = (size_t)n * n;

    // Allocate buffer for the matrix
    double *A = malloc(count * sizeof(double));
    if (!A) exit(1);

    // Read matrix data from parent
    if (read_full(read_fd, A, count * sizeof(double)) != (ssize_t)(count * sizeof(double))) {
        free(A);
        exit(1);
    }

    // Build a temporary Matrix structure (int-based) to reuse compute_determinant()
    Matrix temp;
    temp.rows = n;
    temp.cols = n;
    temp.data = malloc(n * sizeof(int *));
    for (int i = 0; i < n; i++)
        temp.data[i] = malloc(n * sizeof(int));

    // Copy values from A (double) to temp.data (int)
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            temp.data[i][j] = (int)A[i*n + j];

    // Compute determinant using PLU decomposition
    int ok = 0;
    double det = compute_determinant(&temp, &ok);

    // Prepare result header
    rh.success = ok ? 1 : 0;
    rh.rows = 1;
    rh.cols = 1;

    // Send determinant result back to parent
    double result[1] = {det};
    child_send_result(write_fd, &rh, result);

    // Free temporary matrix memory
    for (int i = 0; i < n; i++)
        free(temp.data[i]);
    free(temp.data);
    free(A);
    exit(0);
}

// Child process work for eigenvalue computation
void eigen_child_work(int read_fd, int write_fd) {
    // Read task header from parent
    TaskHeader hdr;
    if (read_full(read_fd, &hdr, sizeof(hdr)) != sizeof(hdr))
        exit(1);

    // Prepare result header for response
    ResultHeader rh = {0};

    // Eigenvalues require a square matrix
    if (hdr.rowsA != hdr.colsA) {
        rh.success = 0;
        rh.rows = hdr.rowsA;
        rh.cols = hdr.colsA;
        child_send_result(write_fd, &rh, NULL);
        exit(0);
    }

    int n = hdr.rowsA;
    size_t count = (size_t)n * n;

    // Allocate matrix data buffer
    double *A = malloc(count * sizeof(double));
    if (!A) exit(1);

    // Read matrix elements from parent
    if (read_full(read_fd, A, count * sizeof(double)) != (ssize_t)(count * sizeof(double))) {
        free(A);
        exit(1);
    }

    // Log computation start
    printf("[Child %d] Computing eigenvalue (type=%d) for %dx%d matrix with OpenMP...\n", getpid(), hdr.op_code, n, n);

    EigenResult eigen_result;

    // Choose algorithm based on op_code
    if (hdr.op_code == 14) {
        // Power iteration for largest eigenvalue
        eigen_result = power_iteration_omp(A, n, 2000, 1e-9);
    } else if (hdr.op_code == 15) {
        // Inverse iteration for smallest eigenvalue
        eigen_result = inverse_iteration_omp(A, n, 2000, 1e-9);
    } else {
        // Invalid operation code
        rh.success = 0;
        child_send_result(write_fd, &rh, NULL);
        free(A);
        exit(0);
    }

    // Display convergence and timing info
    printf("[Child %d] %s in %d iterations (%.6f sec)\n", 
           getpid(),
           eigen_result.converged ? "Converged" : "Max iterations reached",
           eigen_result.iterations,
           eigen_result.time_taken);

    // Print eigenvalue if available
    if (eigen_result.converged) {
        printf("[Child %d] λ = %.6f %s %.6fi\n", 
               getpid(),
               eigen_result.eigenvalue.real,
               eigen_result.eigenvalue.imag >= 0 ? "+" : "",
               eigen_result.eigenvalue.imag);
    }

    // Prepare result header for sending data back
    rh.success = eigen_result.converged ? 1 : 0;
    rh.rows = n + 3;  // 3 metadata values + n vector components
    rh.cols = 1;

    // Allocate array for eigenvalue, iteration count, and vector
    double *result_data = malloc(rh.rows * sizeof(double));
    if (!result_data) {
        free_eigen_result(&eigen_result);
        free(A);
        exit(1);
    }

    // Store: [real, imag, iterations, eigenvector...]
    result_data[0] = eigen_result.eigenvalue.real;
    result_data[1] = eigen_result.eigenvalue.imag;
    result_data[2] = (double)eigen_result.iterations;
    for (int i = 0; i < n; i++) {
        result_data[3 + i] = eigen_result.eigenvector[i];
    }

    // Send result back to parent
    child_send_result(write_fd, &rh, result_data);

    // Ensure parent receives EOF and unblocks read()
    fsync(write_fd);
    close(write_fd);

    // Free allocated memory
    free(result_data);
    free_eigen_result(&eigen_result);
    free(A);

    exit(0);
}
