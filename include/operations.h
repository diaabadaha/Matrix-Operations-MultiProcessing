#ifndef OPERATIONS_H
#define OPERATIONS_H

#include "config.h"

// Complex number for eigenvalues
typedef struct {
    double real;
    double imag;
} ComplexNumber;

// Eigenvalue/Eigenvector result structure
typedef struct {
    ComplexNumber eigenvalue;
    double *eigenvector;
    int converged;
    int iterations;
    double time_taken;
} EigenResult;

// Determinant functions
double compute_determinant(Matrix *m, int *ok);
double determinant_plu(double *A, int n, int *ok);

// Eigenvalue functions
EigenResult power_iteration_omp(double *A, int n, int max_iter, double tolerance);
EigenResult inverse_iteration_omp(double *A, int n, int max_iter, double tolerance);
void deflate_matrix(double *A, int n, ComplexNumber lambda, double *v);
int is_symmetric(Matrix *m);
void free_eigen_result(EigenResult *result);

#endif