#include "template.h"

//Claude helped me with loading in the materials.
// VoxLoader.cpp  —  lives in lib/ alongside ogt_vox.h
// ogt_vox.h can be downloaded from: https://github.com/jpaver/opengametools

#define OGT_VOX_IMPLEMENTATION
#include "ogt_vox.h"
#include "VoxLoader.h"

#include <stdio.h>
#include <string.h>
#include <algorithm>


static constexpr unsigned int VOX_MAT_COUNT = MAT_COUNT; // 3
bool VoxLoader::Load(const char* path, uint* grid, int gridSize, Material* materials)
{
    // --- 1. Read the whole file into memory ---
    FILE* f = fopen(path, "rb");
    if (!f)
    {
        printf("[VoxLoader] Could not open: %s\n", path);
        return false;
    }

    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    rewind(f);

    unsigned char* buffer = new unsigned char[fileSize];
    fread(buffer, 1, fileSize, f);
    fclose(f);

    // --- 2. Parse with ogt_vox ---
    const ogt_vox_scene* scene = ogt_vox_read_scene(buffer, (unsigned int)fileSize);
    delete[] buffer;

    if (!scene)
    {
        printf("[VoxLoader] Failed to parse: %s\n", path);
        return false;
    }

    // --- 3. Build colour lookup table (palette index 0 = empty) ---
    unsigned int colorLUT[256] = {};
    for (int i = 1; i < 256; i++)
    {
        const ogt_vox_rgba& c = scene->palette.color[i];
        const ogt_vox_matl& m = scene->materials.matl[i];

        // Build Material entry at palette offset
        Material& mat = materials[MAT_COUNT + i];
        mat.albedo = { c.r / 255.f, c.g / 255.f, c.b / 255.f };

        switch (m.type)
        {
        case ogt_matl_type_metal:
            mat.type = MaterialType::Metal;
            mat.metallic = 1.0f;
            mat.roughness = (m.content_flags & k_ogt_vox_matl_have_rough) ? m.rough : 0.1f;
            mat.F0 = lerp(float3{ 0.04f,0.04f,0.04f }, mat.albedo, mat.metallic);
            break;
        case ogt_matl_type_glass:
            mat.type = MaterialType::Dielectric;
            mat.ior = (m.content_flags & k_ogt_vox_matl_have_ior) ? m.ior : 1.5f;
            break;
        case ogt_matl_type_emit:
            mat.type = MaterialType::Emissive;
            mat.emission = mat.albedo;
            mat.emissionStr = (m.content_flags & k_ogt_vox_matl_have_emit) ? m.emit : 1.0f;
            break;
        default:
            mat.type = MaterialType::Lambertian;
            mat.roughness = 1.0f;
            break;
        }

        // Grid stores material index, not raw RGB
        colorLUT[i] = MAT_COUNT + i;
    }

    // --- 4. Clear grid ---
    int gridSize2 = gridSize * gridSize;
    int gridSize3 = gridSize * gridSize * gridSize;
    memset(grid, 0, gridSize3 * sizeof(unsigned int));

    int outOfBoundsCount = 0;

    // --- 5. Iterate every instance ---
    for (unsigned int inst = 0; inst < scene->num_instances; inst++)
    {
        const ogt_vox_instance& instance = scene->instances[inst];
        const ogt_vox_model* model = scene->models[instance.model_index];

        if (!model) continue;

        unsigned int sizeX = model->size_x;
        unsigned int sizeY = model->size_y;
        unsigned int sizeZ = model->size_z;

        // Pivot = centre of the model in MagicaVoxel local space
        float pivotX = sizeX * 0.5f;
        float pivotY = sizeY * 0.5f;
        float pivotZ = sizeZ * 0.5f;

        // The instance transform is a flat 4x4 row-major float matrix
        const float* T = &instance.transform.m00;

        // --- 6. Place each non-empty voxel ---
        for (unsigned int z = 0; z < sizeZ; z++)
            for (unsigned int y = 0; y < sizeY; y++)
                for (unsigned int x = 0; x < sizeX; x++)
                {
                    // ogt_vox voxel data is stored x -> y -> z
                    unsigned int voxelIndex = x + y * sizeX + z * sizeX * sizeY;
                    unsigned char colorIndex = model->voxel_data[voxelIndex];
                    if (colorIndex == 0) continue; // empty voxel

                    // Local position relative to model pivot
                    float lx = (float)x - pivotX;
                    float ly = (float)y - pivotY;
                    float lz = (float)z - pivotZ;

                    // Apply instance transform (row-major 4x4, translation in last column)
                    // T layout: [m00 m01 m02 m03 | m10 m11 m12 m13 | m20 m21 m22 m23 | ...]
                    const ogt_vox_transform& T = instance.transform;
                    float wx = T.m00 * lx + T.m10 * ly + T.m20 * lz + T.m30;
                    float wy = T.m01 * lx + T.m11 * ly + T.m21 * lz + T.m31;
                    float wz = T.m02 * lx + T.m12 * ly + T.m22 * lz + T.m32;

                    // MagicaVoxel is Z-up, right-handed.
                    // Your renderer is Y-up.
                    // Conversion: engine(x, y, z) = vox(x, z, y)
                    float ex = wx;
                    float ey = wz; // vox Z  → engine Y
                    float ez = wy; // vox Y  → engine Z

                    // Map to grid index (centre the scene in the 128^3 grid)
                    int gx = (int)(ex + gridSize * 0.5f);
                    int gy = (int)(ey + gridSize * 0.5f);
                    int gz = (int)(ez + gridSize * 0.5f);

                    // Clip to valid range
                    if (gx < 0 || gx >= gridSize ||
                        gy < 0 || gy >= gridSize ||
                        gz < 0 || gz >= gridSize)
                    {
                        outOfBoundsCount++;
                        continue;
                    }

                    // Last-write-wins for overlapping voxels
                    grid[gx + gy * gridSize + gz * gridSize2] = colorLUT[colorIndex];
                }
    }

    if (outOfBoundsCount > 0)
        printf("[VoxLoader] %d voxels were outside the %d^3 grid and skipped.\n",
            outOfBoundsCount, gridSize);

    ogt_vox_destroy_scene(scene);

    // DEBUG — print what the loader actually stored for palette index 143
    printf("[VoxLoader] colorLUT[143] = 0x%08X\n", colorLUT[143]);
    printf("[VoxLoader] materials[MAT_COUNT+143].albedo = %.2f %.2f %.2f\n",
        materials[MAT_COUNT + 143].albedo.x,
        materials[MAT_COUNT + 143].albedo.y,
        materials[MAT_COUNT + 143].albedo.z);

    printf("[VoxLoader] Loaded '%s' successfully.\n", path);
    return true;
}
