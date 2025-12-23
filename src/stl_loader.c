#include "stl_loader.h"
#include "raymath.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifdef _WIN32
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif

//------------------------------------------------------------------------------
// STL Binary Triangle Structure
//------------------------------------------------------------------------------
#pragma pack(push, 1)
typedef struct {
    Vector3 normal;
    Vector3 v1, v2, v3;
    uint16_t attribute;
} STLTriangle;
#pragma pack(pop)

//------------------------------------------------------------------------------
// Helper Functions
//------------------------------------------------------------------------------

bool IsSTLFile(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext)
        return false;
    return (strcasecmp(ext, ".stl") == 0);
}

// Check if STL file is ASCII format (vs binary)
static bool IsASCIISTL(FILE *file, long fileSize) {
    char header[6];
    fseek(file, 0, SEEK_SET);
    if (fread(header, 1, 6, file) != 6)
        return false;

    // ASCII STL starts with "solid "
    if (strncmp(header, "solid ", 6) != 0)
        return false;

    // Binary STL can also start with "solid" in header, so verify by file size
    // Binary format: 80 header + 4 count + (50 * triangles)
    fseek(file, 80, SEEK_SET);
    uint32_t triangleCount = 0;
    if (fread(&triangleCount, sizeof(uint32_t), 1, file) != 1)
        return true; // Assume ASCII if can't read

    long expectedBinarySize = 84 + (long)triangleCount * 50;
    // If file size matches binary format, it's binary despite "solid" header
    if (fileSize == expectedBinarySize)
        return false;

    return true; // ASCII format
}

// Count triangles in ASCII STL by counting "facet" keywords
static uint32_t CountASCIITriangles(FILE *file) {
    uint32_t count = 0;
    char line[256];
    fseek(file, 0, SEEK_SET);

    while (fgets(line, sizeof(line), file)) {
        char *p = line;
        while (*p == ' ' || *p == '\t')
            p++;
        if (strncmp(p, "facet ", 6) == 0)
            count++;
    }
    return count;
}

// Parse ASCII STL file
static Model LoadSTLASCII(const char *path, FILE *file, uint32_t triangleCount) {
    Model model = {0};

    TraceLog(LOG_INFO, "STL: Loading %u triangles (ASCII) from %s", triangleCount, path);

    Mesh mesh = {0};
    mesh.vertexCount = triangleCount * 3;
    mesh.triangleCount = triangleCount;
    mesh.vertices = (float *)RL_MALLOC(sizeof(float) * 3 * mesh.vertexCount);
    mesh.normals = (float *)RL_MALLOC(sizeof(float) * 3 * mesh.vertexCount);
    mesh.texcoords = (float *)RL_CALLOC(mesh.vertexCount * 2, sizeof(float));

    if (!mesh.vertices || !mesh.normals || !mesh.texcoords) {
        TraceLog(LOG_ERROR, "STL: Failed to allocate mesh memory");
        RL_FREE(mesh.vertices);
        RL_FREE(mesh.normals);
        RL_FREE(mesh.texcoords);
        return model;
    }

    fseek(file, 0, SEEK_SET);
    char line[256];
    uint32_t triIndex = 0;
    float nx = 0, ny = 0, nz = 0;
    int vertexInFacet = 0;

    while (fgets(line, sizeof(line), file) && triIndex < triangleCount) {
        char *p = line;
        while (*p == ' ' || *p == '\t')
            p++;

        if (strncmp(p, "facet normal ", 13) == 0) {
            sscanf(p + 13, "%f %f %f", &nx, &ny, &nz);
            vertexInFacet = 0;
        } else if (strncmp(p, "vertex ", 7) == 0) {
            float x, y, z;
            sscanf(p + 7, "%f %f %f", &x, &y, &z);

            int i = triIndex * 9 + vertexInFacet * 3;
            mesh.vertices[i + 0] = x;
            mesh.vertices[i + 1] = y;
            mesh.vertices[i + 2] = z;
            mesh.normals[i + 0] = nx;
            mesh.normals[i + 1] = ny;
            mesh.normals[i + 2] = nz;

            vertexInFacet++;
            if (vertexInFacet == 3)
                triIndex++;
        }
    }

    fclose(file);
    UploadMesh(&mesh, false);

    model.transform = MatrixIdentity();
    model.meshCount = 1;
    model.materialCount = 1;
    model.meshes = (Mesh *)RL_MALLOC(sizeof(Mesh));
    model.materials = (Material *)RL_MALLOC(sizeof(Material));
    model.meshMaterial = (int *)RL_MALLOC(sizeof(int));
    model.meshes[0] = mesh;
    model.materials[0] = LoadMaterialDefault();
    model.meshMaterial[0] = 0;

    TraceLog(LOG_INFO, "STL: Loaded successfully (%d vertices)", mesh.vertexCount);
    return model;
}

// Parse binary STL file
static Model LoadSTLBinary(const char *path, FILE *file, uint32_t triangleCount) {
    Model model = {0};

    TraceLog(LOG_INFO, "STL: Loading %u triangles (binary) from %s", triangleCount, path);

    Mesh mesh = {0};
    mesh.vertexCount = triangleCount * 3;
    mesh.triangleCount = triangleCount;
    mesh.vertices = (float *)RL_MALLOC(sizeof(float) * 3 * mesh.vertexCount);
    mesh.normals = (float *)RL_MALLOC(sizeof(float) * 3 * mesh.vertexCount);
    mesh.texcoords = (float *)RL_CALLOC(mesh.vertexCount * 2, sizeof(float));

    if (!mesh.vertices || !mesh.normals || !mesh.texcoords) {
        TraceLog(LOG_ERROR, "STL: Failed to allocate mesh memory");
        RL_FREE(mesh.vertices);
        RL_FREE(mesh.normals);
        RL_FREE(mesh.texcoords);
        return model;
    }

    STLTriangle tri;
    for (uint32_t t = 0; t < triangleCount; t++) {
        if (fread(&tri, sizeof(STLTriangle), 1, file) != 1) {
            TraceLog(LOG_ERROR, "STL: Failed to read triangle %u", t);
            RL_FREE(mesh.vertices);
            RL_FREE(mesh.normals);
            RL_FREE(mesh.texcoords);
            fclose(file);
            return model;
        }

        int i = t * 9;
        mesh.vertices[i + 0] = tri.v1.x;
        mesh.vertices[i + 1] = tri.v1.y;
        mesh.vertices[i + 2] = tri.v1.z;
        mesh.vertices[i + 3] = tri.v2.x;
        mesh.vertices[i + 4] = tri.v2.y;
        mesh.vertices[i + 5] = tri.v2.z;
        mesh.vertices[i + 6] = tri.v3.x;
        mesh.vertices[i + 7] = tri.v3.y;
        mesh.vertices[i + 8] = tri.v3.z;

        mesh.normals[i + 0] = tri.normal.x;
        mesh.normals[i + 1] = tri.normal.y;
        mesh.normals[i + 2] = tri.normal.z;
        mesh.normals[i + 3] = tri.normal.x;
        mesh.normals[i + 4] = tri.normal.y;
        mesh.normals[i + 5] = tri.normal.z;
        mesh.normals[i + 6] = tri.normal.x;
        mesh.normals[i + 7] = tri.normal.y;
        mesh.normals[i + 8] = tri.normal.z;
    }

    fclose(file);
    UploadMesh(&mesh, false);

    model.transform = MatrixIdentity();
    model.meshCount = 1;
    model.materialCount = 1;
    model.meshes = (Mesh *)RL_MALLOC(sizeof(Mesh));
    model.materials = (Material *)RL_MALLOC(sizeof(Material));
    model.meshMaterial = (int *)RL_MALLOC(sizeof(int));
    model.meshes[0] = mesh;
    model.materials[0] = LoadMaterialDefault();
    model.meshMaterial[0] = 0;

    TraceLog(LOG_INFO, "STL: Loaded successfully (%d vertices)", mesh.vertexCount);
    return model;
}

//------------------------------------------------------------------------------
// Public API
//------------------------------------------------------------------------------

Model LoadSTL(const char *path) {
    Model model = {0};

    FILE *file = fopen(path, "rb");
    if (!file) {
        TraceLog(LOG_ERROR, "STL: Failed to open file: %s", path);
        return model;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (IsASCIISTL(file, fileSize)) {
        uint32_t triangleCount = CountASCIITriangles(file);
        if (triangleCount == 0) {
            TraceLog(LOG_ERROR, "STL: No triangles in ASCII file");
            fclose(file);
            return model;
        }
        return LoadSTLASCII(path, file, triangleCount);
    } else {
        // Binary STL
        fseek(file, 80, SEEK_SET);
        uint32_t triangleCount = 0;
        if (fread(&triangleCount, sizeof(uint32_t), 1, file) != 1) {
            TraceLog(LOG_ERROR, "STL: Failed to read triangle count");
            fclose(file);
            return model;
        }
        if (triangleCount == 0) {
            TraceLog(LOG_ERROR, "STL: No triangles in binary file");
            fclose(file);
            return model;
        }
        return LoadSTLBinary(path, file, triangleCount);
    }
}
