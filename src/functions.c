#include "../include/header.h"
#include <dirent.h>
#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

// Remove file extension from filename
void removeExtension(char *filename) {
    char *dot = strrchr(filename, '.');
    if (dot) *dot = '\0';
}

// Display all matrices in memory
void displayMatrices(const Memory *mem) {
    if (mem->count == 0) {
        printf("\nNo matrices stored in memory.\n");
        return;
    }

    printf("\n===== Stored Matrices =====\n");

    for (int m = 0; m < mem->count; m++) {
        const Matrix *mat = &mem->matrices[m];
        printf("\nMatrix %s (%dx%d):\n", mat->name, mat->rows, mat->cols);
        
        for (int i = 0; i < mat->rows; i++) {
            for (int j = 0; j < mat->cols; j++) {
                printf("%d ", mat->data[i][j]);
            }
            printf("\n");
        }
    }

    printf("===========================\n");
}

// Display a specific matrix by name
void displayMatrixByName(const Memory *mem, const char *name) {
    for (int m = 0; m < mem->count; m++) {
        const Matrix *mat = &mem->matrices[m];
        if (strcmp(mat->name, name) == 0) {
            printf("\nMatrix %s (%dx%d):\n", mat->name, mat->rows, mat->cols);
            for (int i = 0; i < mat->rows; i++) {
                for (int j = 0; j < mat->cols; j++) {
                    printf("%d ", mat->data[i][j]);
                }
                printf("\n");
            }
            return;
        }
    }
    printf("Matrix %s not found in memory.\n", name);
}

// Load all matrices from a folder
void loadMatricesFromFolder(const char *folderPath, Memory *mem) {
    DIR *dir;
    struct dirent *entry;
    char filePath[PATH_MAX];
    char line[1024];

    dir = opendir(folderPath);
    if (!dir) {
        perror("Failed to open directory");
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        // Skip current and parent directory
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        // Check if memory is full
        if (mem->count >= MAX_MATRICES) {
            printf("Reached maximum number of matrices (%d)\n", MAX_MATRICES);
            break;
        }

        snprintf(filePath, sizeof(filePath), "%s/%s", folderPath, entry->d_name);
        FILE *file = fopen(filePath, "r");
        if (!file) {
            perror("Cannot open file");
            continue;
        }

        // Read first line for dimensions
        int rows = 0, cols = 0;
        if (fgets(line, sizeof(line), file) == NULL) {
            printf("Skipping empty file: %s\n", entry->d_name);
            fclose(file);
            continue;
        }

        if (sscanf(line, "%d %d", &rows, &cols) != 2 || rows <= 0 || cols <= 0) {
            printf("Invalid matrix size in file: %s\n", entry->d_name);
            fclose(file);
            continue;
        }

        // Allocate memory for matrix data
        int **tempData = malloc(rows * sizeof(int *));
        for (int i = 0; i < rows; i++) {
            tempData[i] = malloc(cols * sizeof(int));
        }

        int actualRows = 0;
        int invalid = 0;

        // Read matrix data
        while (fgets(line, sizeof(line), file) && actualRows < rows) {
            char *token = strtok(line, " \t\n");
            int colCount = 0;

            while (token && colCount < cols) {
                tempData[actualRows][colCount] = atoi(token);
                token = strtok(NULL, " \t\n");
                colCount++;
            }

            if (colCount != cols) {
                printf("Invalid column count in file: %s (row %d has %d columns, expected %d)\n",
                       entry->d_name, actualRows + 1, colCount, cols);
                invalid = 1;
                break;
            }

            actualRows++;
        }

        // Verify row count
        if (actualRows != rows) {
            printf("Invalid row count in file: %s (got %d, expected %d)\n",
                   entry->d_name, actualRows, rows);
            invalid = 1;
        }

        fclose(file);

        // Free memory if invalid
        if (invalid) {
            for (int i = 0; i < rows; i++)
                free(tempData[i]);
            free(tempData);
            continue;
        }

        // Store matrix in memory
        Matrix *m = &mem->matrices[mem->count];
        strcpy(m->name, entry->d_name);
        removeExtension(m->name);
        m->rows = rows;
        m->cols = cols;
        m->data = tempData;
        mem->count++;

        printf("Loaded matrix: %s (%dx%d)\n", m->name, m->rows, m->cols);
    }

    closedir(dir);
}

// Save all matrices to a folder
void saveMatricesToFolder(const char *folderPath, const Memory *mem) {
    if (mem->count == 0) {
        printf("[Save] No matrices to save.\n");
        return;
    }

    // Create directory if it doesn't exist
    struct stat st = {0};
    if (stat(folderPath, &st) == -1) {
        mkdir(folderPath, 0755);
    }

    // Remove old matrix files that no longer exist in memory
    DIR *dir = opendir(folderPath);
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

            // Check if file corresponds to current matrix
            int found = 0;
            char filename[MAX_NAME_LEN];
            strcpy(filename, entry->d_name);

            char *dot = strrchr(filename, '.');
            if (dot) *dot = '\0';

            for (int i = 0; i < mem->count; i++) {
                if (strcmp(filename, mem->matrices[i].name) == 0) {
                    found = 1;
                    break;
                }
            }

            if (!found) {
                char filePath[PATH_MAX];
                snprintf(filePath, sizeof(filePath), "%s/%s", folderPath, entry->d_name);
                if (remove(filePath) == 0)
                    printf("[Save] Removed old file: %s\n", entry->d_name);
                else
                    perror("Failed to remove old file");
            }
        }
        closedir(dir);
    }

    // Save all current matrices
    for (int m = 0; m < mem->count; m++) {
        const Matrix *mat = &mem->matrices[m];
        char filePath[PATH_MAX];
        snprintf(filePath, sizeof(filePath), "%s/%s.txt", folderPath, mat->name);

        FILE *file = fopen(filePath, "w");
        if (!file) {
            perror("Failed to open file for writing");
            continue;
        }

        // Write dimensions
        fprintf(file, "%d %d\n", mat->rows, mat->cols);

        // Write matrix data
        for (int i = 0; i < mat->rows; i++) {
            for (int j = 0; j < mat->cols; j++) {
                fprintf(file, "%d ", mat->data[i][j]);
            }
            fprintf(file, "\n");
        }

        fclose(file);
        printf("[Save] Saved matrix '%s' to %s\n", mat->name, filePath);
    }

    printf("[Save] All matrices saved to %s\n", folderPath);
}

// Save a single matrix to a file
void saveMatrixToFile(const char *folderPath, const Matrix *mat) {
    // Create folder if needed
    struct stat st = {0};
    if (stat(folderPath, &st) == -1) {
        mkdir(folderPath, 0755);
    }

    char filePath[PATH_MAX];
    snprintf(filePath, sizeof(filePath), "%s/%s.txt", folderPath, mat->name);

    FILE *file = fopen(filePath, "w");
    if (!file) {
        perror("Failed to open file for saving matrix");
        return;
    }

    // Write dimensions
    fprintf(file, "%d %d\n", mat->rows, mat->cols);

    // Write matrix data
    for (int i = 0; i < mat->rows; i++) {
        for (int j = 0; j < mat->cols; j++) {
            fprintf(file, "%d ", mat->data[i][j]);
        }
        fprintf(file, "\n");
    }

    fclose(file);
    printf("[Save] Matrix '%s' saved to %s\n", mat->name, filePath);
}

// Load a single matrix from a file
void loadMatrixFromFile(const char *filePath, Memory *mem) {
    if (mem->count >= MAX_MATRICES) {
        printf("Reached maximum number of matrices (%d)\n", MAX_MATRICES);
        return;
    }

    FILE *file = fopen(filePath, "r");
    if (!file) {
        perror("Cannot open file");
        return;
    }

    int rows, cols;
    if (fscanf(file, "%d %d", &rows, &cols) != 2) {
        printf("Invalid matrix header in file: %s\n", filePath);
        fclose(file);
        return;
    }

    // Allocate matrix data
    int **tempData = malloc(rows * sizeof(int *));
    for (int i = 0; i < rows; i++)
        tempData[i] = malloc(cols * sizeof(int));

    // Read matrix values
    for (int i = 0; i < rows; i++)
        for (int j = 0; j < cols; j++)
            fscanf(file, "%d", &tempData[i][j]);

    fclose(file);

    // Extract matrix name from file path
    char *fname = strrchr(filePath, '/');
    fname = fname ? fname + 1 : (char *)filePath;

    Matrix *m = &mem->matrices[mem->count];
    strcpy(m->name, fname);
    char *dot = strrchr(m->name, '.');
    if (dot) *dot = '\0';

    m->rows = rows;
    m->cols = cols;
    m->data = tempData;
    mem->count++;

    printf("Loaded matrix: %s (%dx%d)\n", m->name, rows, cols);
}
