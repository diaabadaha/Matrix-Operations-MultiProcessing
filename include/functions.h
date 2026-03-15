#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include "header.h"

// Matrix loading and saving
void loadMatricesFromFolder(const char *folderPath, Memory *mem);
void loadMatrixFromFile(const char *filePath, Memory *mem);
void saveMatricesToFolder(const char *folderPath, const Memory *mem);
void saveMatrixToFile(const char *folderPath, const Matrix *matrix);

// Matrix display
void displayMatrices(const Memory *mem);
void displayMatrixByName(const Memory *mem, const char *name);

#endif