#include "../include/header.h"
#include "../include/pipes.h"

// Child process loop - handles individual operations
void child_loop(int read_fd, int write_fd) {
    while (1) {
        TaskHeader hdr;
        double *A = NULL, *B = NULL;

        // Read task from parent
        if (child_read_task(read_fd, &hdr, &A, &B) < 0) {
            free(A); 
            free(B);
            exit(1);
        }

        // Exit command
        if (hdr.op_code == 255) {
            free(A); 
            free(B);
            exit(0);
        }

        // Prepare result
        ResultHeader rh = {
            .rows = 1,
            .cols = 1,
            .success = 1,
            .r = hdr.r,
            .c = hdr.c
        };
        double out = 0.0;

        // Execute operation
        switch (hdr.op_code) {
            case 10:  // ADD
                out = A[0] + B[0];
                break;
                
            case 11:  // SUB
                out = A[0] - B[0];
                break;
                
            case 12: {  // MUL (dot product)
                int k = hdr.colsA;
                double acc = 0.0;
                for (int i = 0; i < k; i++) {
                    acc += A[i] * B[i];
                }
                out = acc;
                break;
            }
                
            default:
                rh.success = 0;
                break;
        }

        // Send result back to parent
        child_send_result(write_fd, &rh, &out);
        free(A); 
        free(B);
    }
}
