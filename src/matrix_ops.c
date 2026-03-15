#include "../include/header.h"

// Helper: PLU decomposition to compute determinant with OpenMP
double determinant_plu(double *A, int n, int *ok) {
    int sign = 1;
    *ok = 1;

    int max_threads = omp_get_max_threads();
    if (n < 50) omp_set_num_threads(2);
    else if (n < 200) omp_set_num_threads(max_threads / 2);
    else omp_set_num_threads(max_threads);

    double start_time = omp_get_wtime();

    for (int k = 0; k < n; k++) {
        // Pivoting
        int pivot = k;
        double maxval = fabs(A[k*n + k]);
        for (int i = k + 1; i < n; i++) {
            double val = fabs(A[i*n + k]);
            if (val > maxval) {
                maxval = val;
                pivot = i;
            }
        }

        if (maxval == 0.0) {
            *ok = 0;
            return 0.0;
        }

        // Row swap
        if (pivot != k) {
            for (int j = 0; j < n; j++) {
                double tmp = A[k*n + j];
                A[k*n + j] = A[pivot*n + j];
                A[pivot*n + j] = tmp;
            }
            sign = -sign;
        }

        // Elimination step
        #pragma omp parallel for collapse(2) default(none) shared(A, n, k)
        for (int i = k + 1; i < n; i++) {
            for (int j = k + 1; j < n; j++) {
                A[i*n + j] -= (A[i*n + k] / A[k*n + k]) * A[k*n + j];
            }
        }

        // Normalize column below pivot
        #pragma omp parallel for default(none) shared(A, n, k)
        for (int i = k + 1; i < n; i++)
            A[i*n + k] /= A[k*n + k];
    }

    double det = (double)sign;
    #pragma omp parallel for reduction(*:det) default(none) shared(A, n)
    for (int i = 0; i < n; i++)
        det *= A[i*n + i];

    double end_time = omp_get_wtime();
    printf("[Child %d] Determinant computed in %.6f sec using %d threads.\n", getpid(), end_time - start_time, omp_get_max_threads());

    return det;
}

// Helper: Compute determinant of matrix
double compute_determinant(Matrix *m, int *ok) {
    if (m->rows != m->cols) {
        *ok = 0;
        return 0.0;
    }

    int n = m->rows;
    size_t count = (size_t)n * n;
    double *A = malloc(count * sizeof(double));
    if (!A) {
        *ok = 0;
        return 0.0;
    }

    #pragma omp parallel for collapse(2) default(none) shared(A, m, n)
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            A[i*n + j] = (double)m->data[i][j];

    double det = determinant_plu(A, n, ok);
    free(A);
    return det;
}

// Helper: Check if matrix is symmetric
int is_symmetric(Matrix *m) {
    if (m->rows != m->cols) return 0;
    
    for (int i = 0; i < m->rows; i++) {
        for (int j = i + 1; j < m->cols; j++) {
            if (m->data[i][j] != m->data[j][i]) return 0;
        }
    }
    return 1;
}

// Helper: Matrix-vector multiplication with OpenMP (only for large matrices)
static void matvec_mult_omp(double *A, double *v, double *result, int n) {
    // Only use OpenMP for matrices >= 100x100
    if (n >= 100) {
        #pragma omp parallel for schedule(static)
        for (int i = 0; i < n; i++) {
            double sum = 0.0;
            for (int j = 0; j < n; j++) {
                sum += A[i * n + j] * v[j];
            }
            result[i] = sum;
        }
    } else {
        // Serial version for small matrices
        for (int i = 0; i < n; i++) {
            double sum = 0.0;
            for (int j = 0; j < n; j++) {
                sum += A[i * n + j] * v[j];
            }
            result[i] = sum;
        }
    }
}

// Helper: Vector norm
static double vector_norm(double *v, int n) {
    double sum = 0.0;
    for (int i = 0; i < n; i++) {
        sum += v[i] * v[i];
    }
    return sqrt(sum);
}

// Helper: Normalize vector
static void normalize_vector(double *v, int n) {
    double norm = vector_norm(v, n);
    if (norm > 1e-10) {
        for (int i = 0; i < n; i++) {
            v[i] /= norm;
        }
    }
}

// Helper: Dot product
static double dot_product(double *v1, double *v2, int n) {
    double sum = 0.0;
    for (int i = 0; i < n; i++) {
        sum += v1[i] * v2[i];
    }
    return sum;
}

// Power iteration with OpenMP
EigenResult power_iteration_omp(double *A, int n, int max_iter, double tolerance) {
    EigenResult result = {0};
    result.eigenvector = malloc(n * sizeof(double));
    result.converged = 0;
    
    double start_time = omp_get_wtime();
    
    // Initialize random vector
    srand(time(NULL) + getpid());
    for (int i = 0; i < n; i++) {
        result.eigenvector[i] = (double)rand() / RAND_MAX - 0.5;
    }
    normalize_vector(result.eigenvector, n);
    
    double *Av = malloc(n * sizeof(double));
    double lambda_old = 0.0;
    
    for (int iter = 0; iter < max_iter; iter++) {
        // Av = A * v
        matvec_mult_omp(A, result.eigenvector, Av, n);
        
        // lambda = v^T * Av (Rayleigh quotient)
        double lambda_new = dot_product(result.eigenvector, Av, n);
        
        // Normalize Av for next iteration
        normalize_vector(Av, n);
        
        // Check convergence
        if (fabs(lambda_new - lambda_old) < tolerance) {
            result.eigenvalue.real = lambda_new;
            result.eigenvalue.imag = 0.0;
            result.converged = 1;
            result.iterations = iter + 1;
            
            // Copy normalized vector
            memcpy(result.eigenvector, Av, n * sizeof(double));
            break;
        }
        
        lambda_old = lambda_new;
        memcpy(result.eigenvector, Av, n * sizeof(double));
    }
    
    if (!result.converged) {
        result.eigenvalue.real = lambda_old;
        result.eigenvalue.imag = 0.0;
        result.iterations = max_iter;
    }
    
    free(Av);
    
    double end_time = omp_get_wtime();
    result.time_taken = end_time - start_time;
    
    return result;
}

// Helper: Solve linear system Ax = b using Gaussian elimination
static int solve_linear_system(double *A, double *b, double *x, int n) {
    // Create augmented matrix
    double *aug = malloc(n * (n + 1) * sizeof(double));
    
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            aug[i * (n + 1) + j] = A[i * n + j];
        }
        aug[i * (n + 1) + n] = b[i];
    }
    
    // Forward elimination
    for (int k = 0; k < n; k++) {
        // Find pivot
        int pivot = k;
        double max_val = fabs(aug[k * (n + 1) + k]);
        for (int i = k + 1; i < n; i++) {
            if (fabs(aug[i * (n + 1) + k]) > max_val) {
                max_val = fabs(aug[i * (n + 1) + k]);
                pivot = i;
            }
        }
        
        if (max_val < 1e-10) {
            free(aug);
            return 0; // Singular matrix
        }
        
        // Swap rows
        if (pivot != k) {
            for (int j = 0; j < n + 1; j++) {
                double tmp = aug[k * (n + 1) + j];
                aug[k * (n + 1) + j] = aug[pivot * (n + 1) + j];
                aug[pivot * (n + 1) + j] = tmp;
            }
        }
        
        // Eliminate column
        for (int i = k + 1; i < n; i++) {
            double factor = aug[i * (n + 1) + k] / aug[k * (n + 1) + k];
            for (int j = k; j < n + 1; j++) {
                aug[i * (n + 1) + j] -= factor * aug[k * (n + 1) + j];
            }
        }
    }
    
    // Back substitution
    for (int i = n - 1; i >= 0; i--) {
        x[i] = aug[i * (n + 1) + n];
        for (int j = i + 1; j < n; j++) {
            x[i] -= aug[i * (n + 1) + j] * x[j];
        }
        x[i] /= aug[i * (n + 1) + i];
    }
    
    free(aug);
    return 1;
}

// Inverse iteration with OpenMP
EigenResult inverse_iteration_omp(double *A, int n, int max_iter, double tolerance) {
    EigenResult result = {0};
    result.eigenvector = malloc(n * sizeof(double));
    result.converged = 0;
    
    double start_time = omp_get_wtime();
    
    // Initialize random vector
    srand(time(NULL) + getpid() + 1);
    for (int i = 0; i < n; i++) {
        result.eigenvector[i] = (double)rand() / RAND_MAX - 0.5;
    }
    normalize_vector(result.eigenvector, n);
    
    double *x_new = malloc(n * sizeof(double));
    double lambda_old = 0.0;
    
    for (int iter = 0; iter < max_iter; iter++) {
        // Solve A * x_new = v
        if (!solve_linear_system(A, result.eigenvector, x_new, n)) {
            result.converged = 0;
            result.iterations = iter;
            break;
        }
        
        // Normalize
        normalize_vector(x_new, n);
        
        // Rayleigh quotient: lambda = v^T * A * v
        double *Av = malloc(n * sizeof(double));
        matvec_mult_omp(A, x_new, Av, n);
        double lambda_new = dot_product(x_new, Av, n);
        free(Av);
        
        // Check convergence
        if (fabs(lambda_new - lambda_old) < tolerance) {
            result.eigenvalue.real = lambda_new;
            result.eigenvalue.imag = 0.0;
            result.converged = 1;
            result.iterations = iter + 1;
            memcpy(result.eigenvector, x_new, n * sizeof(double));
            break;
        }
        
        lambda_old = lambda_new;
        memcpy(result.eigenvector, x_new, n * sizeof(double));
    }
    
    if (!result.converged) {
        result.eigenvalue.real = lambda_old;
        result.eigenvalue.imag = 0.0;
        result.iterations = max_iter;
    }
    
    free(x_new);
    
    double end_time = omp_get_wtime();
    result.time_taken = end_time - start_time;
    
    return result;
}

// Matrix deflation
void deflate_matrix(double *A, int n, ComplexNumber lambda, double *v) {    
    if (fabs(lambda.imag) < 1e-10) {
        // Real eigenvalue - simple deflation
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                A[i * n + j] -= lambda.real * v[i] * v[j];
            }
        }
    }
}

// Free EigenResult resources
void free_eigen_result(EigenResult *result) {
    if (result && result->eigenvector) {
        free(result->eigenvector);
        result->eigenvector = NULL;
    }
}