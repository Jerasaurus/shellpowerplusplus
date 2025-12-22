#ifndef STL_LOADER_H
#define STL_LOADER_H

// STL loader implementation
// This header contains implementation-specific structures

#include "raylib.h"
#include <stdbool.h>

// Check if a file path has .stl extension
bool IsSTLFile(const char *path);

// Load an STL file and return a Model
Model LoadSTL(const char *path);

#endif // STL_LOADER_H
