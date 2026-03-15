#include "../include/header.h"
#include <linux/limits.h>

// Store result matrix from parent response
static void store_result_matrix(Memory *mem, const char *resp) {
    if (strncmp(resp, "RESULT_OK", 9) != 0) {
        if (strstr(resp, "Matrix_not_found")) {
            printf("\nError: One or both matrices not found.\n");
        } else if (strstr(resp, "Size_mismatch")) {
            printf("\nError: Matrices have different dimensions.\n");
        } else if (strstr(resp, "Bad_dims")) {
            printf("\nError: Invalid dimensions for multiplication.\n");
        } else {
            printf("\nError: Operation failed - %s\n", resp);
        }
        return;
    }

    // Parse response: "RESULT_OK name rows cols data..."
    char name[MAX_NAME_LEN];
    int rows, cols;
    sscanf(resp + 10, "%s %d %d", name, &rows, &cols);

    // Check if matrix already exists
    int idx = -1;
    for (int i = 0; i < mem->count; i++) {
        if (strcmp(mem->matrices[i].name, name) == 0) {
            idx = i;
            break;
        }
    }

    // Allocate new matrix if needed
    if (idx == -1) {
        if (mem->count >= MAX_MATRICES) {
            printf("\nWarning: Memory full, cannot store result.\n");
            return;
        }
        idx = mem->count++;
        strcpy(mem->matrices[idx].name, name);
    } else {
        // Free old data if exists
        if (mem->matrices[idx].data) {
            for (int i = 0; i < mem->matrices[idx].rows; i++)
                free(mem->matrices[idx].data[i]);
            free(mem->matrices[idx].data);
        }
    }

    Matrix *m = &mem->matrices[idx];
    m->rows = rows;
    m->cols = cols;

    // Allocate data
    m->data = malloc(rows * sizeof(int *));
    for (int i = 0; i < rows; i++)
        m->data[i] = malloc(cols * sizeof(int));

    // Parse matrix values
    char *ptr = strchr(resp, ' ');  // skip "RESULT_OK"
    ptr = strchr(ptr + 1, ' ');     // skip name
    ptr = strchr(ptr + 1, ' ');     // skip rows
    ptr = strchr(ptr + 1, ' ');     // at cols, move to first value
    
    char *token = strtok(ptr + 1, " ");
    for (int i = 0; i < rows && token; i++) {
        for (int j = 0; j < cols && token; j++) {
            m->data[i][j] = atoi(token);
            token = strtok(NULL, " ");
        }
    }

    printf("\n===== Operation Result =====\n");
    printf("Matrix %s (%dx%d):\n", name, rows, cols);
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            printf("%d ", m->data[i][j]);
        }
        printf("\n");
    }
    printf("============================\n");
    printf("Result stored in memory as '%s'\n", name);
}

void run_menu(pid_t parent_pid, Memory *mem, int write_fd, int read_fd)
{
    int choice;
    
    // Small delay to ensure parent initialization completes
    usleep(100000);  // 100ms delay

    while (1)
    {
        printf("\n========= Matrix Operations Menu =========\n");
        printf("1. Enter a matrix\n");
        printf("2. Display a matrix by name\n");
        printf("3. Delete a matrix\n");
        printf("4. Modify a matrix (row, column, or value)\n");
        printf("5. Read a matrix from a file\n");
        printf("6. Read matrices from a folder\n");
        printf("7. Save a matrix to a file\n");
        printf("8. Save all matrices to a folder\n");
        printf("9. Display all matrices\n");
        printf("10. Add two matrices\n");
        printf("11. Subtract two matrices\n");
        printf("12. Multiply two matrices\n");
        printf("13. Find determinant\n");
        printf("14. Find eigenvalues & eigenvectors\n");
        printf("15. Exit\n");
        printf("==========================================\n");
        printf("Enter your choice: ");
        fflush(stdout);

        if (scanf("%d", &choice) != 1)
        {
            printf("Invalid input. Try again.\n");
            while (getchar() != '\n');
            continue;
        }
        getchar();

        // EXIT
        if (choice == 15)
        {
            printf("\nExiting program...\n");
            kill(parent_pid, SIGUSR2);
            break;
        }

        // CREATE MATRIX
        if (choice == 1)
        {
            char name[20];
            int rows, cols;

            printf("Enter matrix name: ");
            scanf("%19s", name);

            // 1. Ensure matrix name is unique
            int duplicate = 0;
            for (int i = 0; i < mem->count; i++)
            {
                if (strcmp(mem->matrices[i].name, name) == 0)
                {
                    duplicate = 1;
                    break;
                }
            }
            if (duplicate)
            {
                printf("Error: Matrix with this name already exists. Choose a different name.\n");
                while (getchar() != '\n'); // clear buffer
                continue;
            }

            printf("Enter rows and columns: ");
            if (scanf("%d %d", &rows, &cols) != 2 || rows <= 0 || cols <= 0)
            {
                printf("Error: Invalid numeric input for dimensions.\n");
                while (getchar() != '\n'); // clear buffer
                continue;
            }

            if (mem->count >= MAX_MATRICES)
            {
                printf("Error: Memory full.\n");
                continue;
            }

            Matrix *newMatrix = &mem->matrices[mem->count];
            strcpy(newMatrix->name, name);
            newMatrix->rows = rows;
            newMatrix->cols = cols;

            newMatrix->data = malloc(rows * sizeof(int *));
            for (int i = 0; i < rows; i++)
                newMatrix->data[i] = malloc(cols * sizeof(int));

            printf("Enter matrix values row by row (%d x %d = %d total values):\n",
                   rows, cols, rows * cols);

            int totalValues = rows * cols;
            int enteredValues = 0;
            int value;

            while (enteredValues < totalValues)
            {
                if (scanf("%d", &value) != 1)
                {
                    printf("Error: Non-numeric input detected. Please re-enter remaining values.\n");
                    while (getchar() != '\n'); // clear invalid input
                    continue;
                }

                int i = enteredValues / cols;
                int j = enteredValues % cols;
                newMatrix->data[i][j] = value;
                enteredValues++;

                if (enteredValues == totalValues)
                {
                    printf("Matrix input complete.\n");
                    break;
                }
            }

            // Prevent extra entries beyond required size
            if (enteredValues >= totalValues)
            {
                printf("You have entered all required %d values. Extra input will be ignored.\n", totalValues);
                while (getchar() != '\n'); // flush any trailing input
            }

            mem->count++;

            // Build CREATE message
            char msg[1024] = "";
            snprintf(msg, sizeof(msg), "CREATE %s %d %d", name, rows, cols);
            for (int i = 0; i < rows; i++)
                for (int j = 0; j < cols; j++)
                {
                    char tmp[32];
                    snprintf(tmp, sizeof(tmp), " %d", newMatrix->data[i][j]);
                    strcat(msg, tmp);
                }

            // Send to parent
            write(write_fd, msg, strlen(msg) + 1);
            kill(parent_pid, SIGUSR1);

            // Wait for response
            char resp[128];
            ssize_t n = read(read_fd, resp, sizeof(resp) - 1);
            if (n > 0)
            {
                resp[n] = '\0';
                printf("[Menu] Parent response: %s\n", resp);
            }
            else
            {
                printf("[Menu] No response from parent.\n");
            }
        }

        // DISPLAY MATRIX BY NAME
        else if (choice == 2)
        {
            char name[20];
            printf("Enter matrix name: ");
            scanf("%19s", name);
            displayMatrixByName(mem, name);
        }

        // DELETE MATRIX
        else if (choice == 3)
        {
            char name[20];
            printf("Enter matrix name to delete: ");
            scanf("%19s", name);

            char msg[128];
            snprintf(msg, sizeof(msg), "DELETE %s", name);
            write(write_fd, msg, strlen(msg) + 1);
            kill(parent_pid, SIGUSR1);

            char resp[128];
            ssize_t n = read(read_fd, resp, sizeof(resp) - 1);
            if (n > 0)
            {
                resp[n] = '\0';
            }

            // Remove locally
            for (int i = 0; i < mem->count; i++)
            {
                if (strcmp(mem->matrices[i].name, name) == 0)
                {
                    for (int r = 0; r < mem->matrices[i].rows; r++)
                        free(mem->matrices[i].data[r]);
                    free(mem->matrices[i].data);
                    for (int j = i; j < mem->count - 1; j++)
                        mem->matrices[j] = mem->matrices[j + 1];
                    mem->count--;
                    printf("Matrix '%s' deleted.\n", name);
                    break;
                }
            }
        }

        // MODIFY MATRIX
        else if (choice == 4)
        {
            char name[20];
            printf("Enter matrix name to modify: ");
            scanf("%19s", name);

            // Verify matrix exists
            int found = 0, rows = 0, cols = 0, idx = -1;
            for (int i = 0; i < mem->count; i++)
            {
                if (strcmp(mem->matrices[i].name, name) == 0)
                {
                    found = 1;
                    idx = i;
                    rows = mem->matrices[i].rows;
                    cols = mem->matrices[i].cols;
                    break;
                }
            }

            if (!found)
            {
                printf("Error: Matrix '%s' not found.\nReturning to main menu...\n", name);
                while (getchar() != '\n'); // clear buffer
                continue;
            }

            // Print current matrix
            printf("\nMatrix '%s' (%dx%d):\n", name, rows, cols);
            for (int i = 0; i < rows; i++)
            {
                for (int j = 0; j < cols; j++)
                    printf("%d ", mem->matrices[idx].data[i][j]);
                printf("\n");
            }

            int subchoice;
            while (1)
            {
                printf("\n===== Modify Matrix '%s' =====\n", name);
                printf("1. Modify full row\n");
                printf("2. Modify full column\n");
                printf("3. Modify single value\n");
                printf("4. Back to main menu\n");
                printf("===============================\n");
                printf("Enter your choice: ");
                fflush(stdout);

                if (scanf("%d", &subchoice) != 1)
                {
                    printf("Invalid input (non-numeric). Try again.\n");
                    while (getchar() != '\n');
                    continue;
                }

                getchar(); // clear newline

                if (subchoice == 4)
                    break;

                char msg[1024] = "";

                // Modify full row
                if (subchoice == 1)
                {
                    int row;
                    printf("Enter row index (0-based): ");
                    if (scanf("%d", &row) != 1)
                    {
                        printf("Error: Invalid input. Row index must be a number.\n");
                        while (getchar() != '\n');
                        continue;
                    }

                    if (row < 0 || row >= rows)
                    {
                        printf("Error: Row index out of range (0 to %d).\n", rows - 1);
                        continue;
                    }

                    printf("Enter %d new numeric values for row %d:\n", cols, row);
                    char values[512] = "";
                    int entered = 0, val;
                    while (entered < cols)
                    {
                        if (scanf("%d", &val) != 1)
                        {
                            printf("Error: Non-numeric value detected. Re-enter remaining values.\n");
                            while (getchar() != '\n');
                            continue;
                        }
                        char tmp[32];
                        sprintf(tmp, " %d", val);
                        strcat(values, tmp);
                        entered++;
                    }
                    printf("Row update complete.\n");
                    snprintf(msg, sizeof(msg), "MODIFY %s ROW %d%s", name, row, values);
                }

                // Modify full column
                else if (subchoice == 2)
                {
                    int col;
                    printf("Enter column index (0-based): ");
                    if (scanf("%d", &col) != 1)
                    {
                        printf("Error: Invalid input. Column index must be a number.\n");
                        while (getchar() != '\n');
                        continue;
                    }

                    if (col < 0 || col >= cols)
                    {
                        printf("Error: Column index out of range (0 to %d).\n", cols - 1);
                        continue;
                    }

                    printf("Enter %d new numeric values for column %d:\n", rows, col);
                    char values[512] = "";
                    int entered = 0, val;
                    while (entered < rows)
                    {
                        if (scanf("%d", &val) != 1)
                        {
                            printf("Error: Non-numeric value detected. Re-enter remaining values.\n");
                            while (getchar() != '\n');
                            continue;
                        }
                        char tmp[32];
                        sprintf(tmp, " %d", val);
                        strcat(values, tmp);
                        entered++;
                    }
                    printf("Column update complete.\n");
                    snprintf(msg, sizeof(msg), "MODIFY %s COL %d%s", name, col, values);
                }

                // Modify single value
                else if (subchoice == 3)
                {
                    int row, col, val;

                    printf("Enter row and column indices (0-based): ");
                    if (scanf("%d %d", &row, &col) != 2)
                    {
                        printf("Error: Invalid numeric input for indices.\n");
                        while (getchar() != '\n');
                        continue;
                    }

                    if (row < 0 || row >= rows || col < 0 || col >= cols)
                    {
                        printf("Error: Indices out of range (rows: 0–%d, cols: 0–%d).\n", rows - 1, cols - 1);
                        continue;
                    }

                    printf("Enter new value: ");
                    if (scanf("%d", &val) != 1)
                    {
                        printf("Error: Value must be numeric.\n");
                        while (getchar() != '\n');
                        continue;
                    }

                    snprintf(msg, sizeof(msg), "MODIFY %s VALUE %d %d %d", name, row, col, val);
                }

                else
                {
                    printf("Invalid sub-choice. Try again.\n");
                    continue;
                }

                // Send modification request to parent
                write(write_fd, msg, strlen(msg) + 1);
                kill(parent_pid, SIGUSR1);

                // Wait for parent response
                char resp[2048];
                ssize_t n = read(read_fd, resp, sizeof(resp) - 1);
                if (n > 0)
                {
                    resp[n] = '\0';
                    printf("[Menu] Parent response: %s\n", resp);

                    if (strncmp(resp, "MODIFY_OK", 9) == 0)
                    {
                        // Parse updated matrix and replace local copy
                        char mname[64];
                        int rcount, ccount;
                        sscanf(resp + 10, "%s %d %d", mname, &rcount, &ccount);

                        Matrix *target = NULL;
                        for (int i = 0; i < mem->count; i++)
                        {
                            if (strcmp(mem->matrices[i].name, mname) == 0)
                            {
                                target = &mem->matrices[i];
                                break;
                            }
                        }

                        if (!target)
                        {
                            if (mem->count >= MAX_MATRICES)
                            {
                                printf("[Menu] Memory full, cannot store updated matrix.\n");
                                break;
                            }
                            target = &mem->matrices[mem->count++];
                            strcpy(target->name, mname);
                        }

                        target->rows = rcount;
                        target->cols = ccount;

                        // Allocate if needed
                        if (!target->data)
                        {
                            target->data = malloc(rcount * sizeof(int *));
                            for (int i = 0; i < rcount; i++)
                                target->data[i] = malloc(ccount * sizeof(int));
                        }

                        // Parse matrix values
                        char *ptr = strchr(resp, ' ');
                        for (int i = 0; i < 3; i++)
                            ptr = strchr(ptr + 1, ' ');
                        char *token = strtok(ptr + 1, " ");
                        for (int i = 0; i < rcount; i++)
                            for (int j = 0; j < ccount && token; j++)
                            {
                                target->data[i][j] = atoi(token);
                                token = strtok(NULL, " ");
                            }

                        printf("[Menu] Matrix '%s' updated locally.\n", mname);

                        // Print updated matrix
                        printf("\nUpdated Matrix '%s' (%dx%d):\n", mname, rcount, ccount);
                        for (int i = 0; i < rcount; i++)
                        {
                            for (int j = 0; j < ccount; j++)
                                printf("%d ", target->data[i][j]);
                            printf("\n");
                        }
                    }
                    else if (strncmp(resp, "MODIFY_FAIL", 11) == 0)
                    {
                        printf("[Menu] Modification failed.\n");
                    }
                }
                else
                {
                    printf("[Menu] No response from parent.\n");
                }

                break; // after one modification, return to main menu
            }
        }

        // READ MATRIX FROM FILE
        else if (choice == 5)
        {
            char path[PATH_MAX];
            printf("Enter file path: ");
            scanf("%s", path);
            loadMatrixFromFile(path, mem);
        }

        // READ MATRICES FROM FOLDER
        else if (choice == 6)
        {
            char folder[PATH_MAX];
            printf("Enter folder path: ");
            scanf("%s", folder);
            loadMatricesFromFolder(folder, mem);
        }

        // SAVE MATRIX TO FILE
        else if (choice == 7)
        {
            char name[20];
            printf("Enter matrix name to save: ");
            scanf("%19s", name);

            int found = 0;
            for (int i = 0; i < mem->count; i++)
            {
                if (strcmp(mem->matrices[i].name, name) == 0)
                {
                    saveMatrixToFile("./data/matrices", &mem->matrices[i]);
                    found = 1;
                    break;
                }
            }

            if (!found)
            {
                printf("Matrix '%s' not found.\n", name);
            }
        }

        // SAVE ALL MATRICES TO FOLDER
        else if (choice == 8)
        {
            printf("Saving all matrices to ./data/matrices...\n");
            saveMatricesToFolder("./data/matrices", mem);
        }

        // DISPLAY ALL MATRICES
        else if (choice == 9)
        {
            displayMatrices(mem);
        }

        // ADD / SUB / MUL
        else if (choice == 10 || choice == 11 || choice == 12)
        {
            char A[64], B[64];
            printf("Enter first matrix name: ");
            scanf("%63s", A);
            printf("Enter second matrix name: ");
            scanf("%63s", B);

            char cmd[256];
            if (choice == 10) snprintf(cmd, sizeof(cmd), "ADD %s %s", A, B);
            else if (choice == 11) snprintf(cmd, sizeof(cmd), "SUB %s %s", A, B);
            else snprintf(cmd, sizeof(cmd), "MUL %s %s", A, B);

            write(write_fd, cmd, strlen(cmd) + 1);
            kill(parent_pid, SIGUSR1);

            char resp[16384];
            ssize_t n = read(read_fd, resp, sizeof(resp) - 1);

            if (n > 0)
            {
                resp[n] = '\0';
                store_result_matrix(mem, resp);
            }
            else
            {
                printf("No response from parent.\n");
            }
        }

        // DETERMINANT
        else if (choice == 13)
        {
            char name[20];
            printf("Enter matrix name: ");
            scanf("%19s", name);

            char msg[64];
            snprintf(msg, sizeof(msg), "DET %s", name);
            write(write_fd, msg, strlen(msg) + 1);
            kill(parent_pid, SIGUSR1);

            char resp[128];
            ssize_t n = read(read_fd, resp, sizeof(resp) - 1);
            if (n > 0)
            {
                resp[n] = '\0';

                if (strncmp(resp, "DET_OK", 6) == 0)
                {
                    char mname[64];
                    double det;
                    sscanf(resp + 7, "%63s %lf", mname, &det);

                    if (fabs(det) < 1e-9)
                        printf("\nDeterminant of %s = 0 (Matrix is singular)\n", mname);
                    else
                        printf("\nDeterminant of %s = %.6f\n", mname, det);
                }
                else if (strncmp(resp, "DET_FAIL", 8) == 0)
                {
                    if (strstr(resp, "Not_square"))
                        printf("\nMatrix is not square; determinant undefined.\n");
                    else if (strstr(resp, "Matrix_not_found"))
                        printf("\nMatrix not found.\n");
                    else if (strstr(resp, "Singular_or_invalid"))
                        printf("\nDeterminant is 0 (Matrix is singular).\n");
                    else
                        printf("\nDeterminant computation failed.\n");
                }
            }
        }

        // EIGENVALUES
        else if (choice == 14)
        {
            char name[20];
            printf("Enter matrix name: ");
            scanf("%19s", name);

            char msg[64];
            snprintf(msg, sizeof(msg), "EIGEN %s", name);
            write(write_fd, msg, strlen(msg) + 1);
            kill(parent_pid, SIGUSR1);

            char resp[2048];
            ssize_t n = read(read_fd, resp, sizeof(resp) - 1);
            if (n > 0)
            {
                resp[n] = '\0';

                if (strncmp(resp, "EIGEN_OK", 8) == 0)
                {
                    int num_eigen = 0;
                    double *eigenvalues_real = malloc(10 * sizeof(double));
                    double *eigenvalues_imag = malloc(10 * sizeof(double));
                    
                    char *ptr = resp + 9;
                    sscanf(ptr, "%d", &num_eigen);
                    
                    ptr = strchr(ptr, ' ');
                    for (int i = 0; i < num_eigen && ptr; i++) {
                        ptr++;
                        sscanf(ptr, "%lf %lf", &eigenvalues_real[i], &eigenvalues_imag[i]);
                        ptr = strchr(ptr, ' ');
                        if (ptr) ptr = strchr(ptr + 1, ' ');
                    }
                    
                }
                else if (strncmp(resp, "EIGEN_FAIL", 10) == 0)
                {
                    if (strstr(resp, "Not_square"))
                        printf("\nMatrix is not square; eigenvalues undefined.\n");
                    else if (strstr(resp, "Matrix_not_found"))
                        printf("\nMatrix not found.\n");
                    else
                        printf("\nEigenvalue computation failed.\n");
                }
            }
        }

        // INVALID CHOICE
        else
        {
            printf("Invalid choice. Please try again.\n");
        }
    }
}