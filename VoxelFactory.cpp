#include "template.h"
#include "VoxelFactory.h"
#include "VoxelObject.h"
#include "VoxelInstance.h"
#include <ogt_vox.h>
#include <cmath>

/// @brief  Creates a new VoxelObject from raw voxel data and registers it
///         in the scene's object list.
///
/// @param scene   The scene to add the object to.
/// @param sizeX/Y/Z  Dimensions of the voxel grid.
/// @param voxels  Flat array of colour indices (0 = empty).
/// @return        Index of the newly created object in scene.voxelObjects.
int VoxelFactory::CreateObject(Tmpl8::Scene& scene, uint sizeX, uint sizeY, uint sizeZ, const std::vector<uint8_t>& voxels)
{
    VoxelObject obj(sizeX, sizeY, sizeZ, voxels);
    int index = (int)scene.voxelObjects.size();
    scene.voxelObjects.push_back(obj);
    return index;
}

/// @brief  Creates a VoxelInstance (transform record) and registers it
///         in the scene without flattening voxels into the world grid.
///
/// Use this when the TLAS/BLAS instancing path is active.  For static
/// scenes that bake voxels directly into the grid, use FlattenInstance.
///
/// @param objectIndex  Index into scene.voxelObjects.
/// @param position     World-space translation.
/// @param rotation     Euler angles in radians (X, Y, Z).
/// @param scale        Per-axis scale factors.
/// @param pivot        Local pivot offset applied before rotation.
void VoxelFactory::CreateInstance(
    Tmpl8::Scene& scene,
    int objectIndex,
    float3 position,
    float3 rotation,
    float3 scale,
    float3 pivot)
{
    VoxelInstance inst(objectIndex, position, rotation, scale, pivot);
    scene.voxelInstances.push_back(inst);
}

/// @brief  Decomposes an ogt_vox 4×4 column-major transform into
///         position, Euler rotation, and scale.
///
/// Rotation is extracted using the ZXY convention.
/// Assumes no shear and approximately uniform scale per axis.
///
/// @param T         Source ogt_vox transform (column-major 4×4).
/// @param outPos    Extracted world-space translation (last column).
/// @param outRot    Extracted Euler angles in radians (X, Y, Z).
/// @param outScale  Extracted per-axis scale (column lengths).
/// @param pivot     Unused pivot parameter (reserved for future use).
void VoxelFactory::FromVoxTransform(const ogt_vox_transform& T, float3& outPos, float3& outRot, float3& outScale, float3 pivot)
{
    // Translation: last column of the 4×4 matrix
    outPos = float3(T.m30, T.m31, T.m32);

    // Scale: length of each rotation column (strips scale from the matrix)
    outScale.x = sqrtf(T.m00 * T.m00 + T.m01 * T.m01 + T.m02 * T.m02);
    outScale.y = sqrtf(T.m10 * T.m10 + T.m11 * T.m11 + T.m12 * T.m12);
    outScale.z = sqrtf(T.m20 * T.m20 + T.m21 * T.m21 + T.m22 * T.m22);

    // Rotation: ZXY Euler extraction from the normalised rotation matrix
    // (dividing by scale converts the columns back to unit vectors)
    outRot.x = atan2f(T.m21 / outScale.z, T.m22 / outScale.z);
    outRot.y = atan2f(-T.m20 / outScale.z, sqrtf(T.m21 * T.m21 + T.m22 * T.m22) / outScale.z);
    outRot.z = atan2f(T.m10 / outScale.y, T.m00 / outScale.x);
}

/// @brief  Bakes a VoxelObject instance directly into the scene's world grid.
///
/// Each voxel in the object is transformed (scaled → rotated Y→X→Z →
/// translated) and written into the flat brick grid via SetVoxel.
/// Out-of-bounds voxels are silently discarded.
///
/// This is a one-shot, destructive operation: voxels become part of the
/// static world and can no longer be moved as a unit.  For dynamic /
/// instanced objects use CreateInstance instead.
///
/// @param scene        The scene whose grid receives the voxels.
/// @param objectIndex  Index into scene.voxelObjects.
/// @param position     World-space translation applied after rotation.
/// @param rotation     Euler angles in radians (Y, X, Z applied in that order).
/// @param scale        Per-axis scale applied in local space before rotation.
void VoxelFactory::FlattenInstance(
    Tmpl8::Scene& scene,
    int objectIndex,
    float3 position,
    float3 rotation,
    float3 scale)
{
    const VoxelObject& obj = scene.voxelObjects[objectIndex];

    // Record the instance transform so the scene graph stays consistent
    scene.voxelInstances.push_back({ objectIndex, position, rotation, scale, float3(0, 0, 0) });

    // Pre-compute sin/cos for all three rotation axes
    float cx = cosf(rotation.x), sx = sinf(rotation.x); // X (pitch)
    float cy = cosf(rotation.y), sy = sinf(rotation.y); // Y (yaw)
    float cz = cosf(rotation.z), sz = sinf(rotation.z); // Z (roll)

    for (uint z = 0; z < obj.sizeZ; z++)
        for (uint y = 0; y < obj.sizeY; y++)
            for (uint x = 0; x < obj.sizeX; x++)
            {
                // Look up the colour index; skip empty voxels
                uint8_t ci = obj.voxels[x + y * obj.sizeX + z * obj.sizeX * obj.sizeY];
                if (ci == 0) continue;

                // Translate to object-centre and apply per-axis scale
                float lx = ((float)x - obj.sizeX * 0.5f) * scale.x;
                float ly = ((float)y - obj.sizeY * 0.5f) * scale.y;
                float lz = ((float)z - obj.sizeZ * 0.5f) * scale.z;

                // --- Rotation order: Y (yaw) first ---
                float rx = cy * lx + sy * lz;
                float ry = ly;
                float rz = -sy * lx + cy * lz;

                // --- Then X (pitch) ---
                float rx2 = rx;
                float ry2 = cx * ry - sx * rz;
                float rz2 = sx * ry + cx * rz;

                // --- Then Z (roll) ---
                float rx3 = cz * rx2 - sz * ry2;
                float ry3 = sz * rx2 + cz * ry2;
                float rz3 = rz2; // Z axis unchanged by Z-rotation

                // Convert to integer world-grid coordinates
                int gx = (int)roundf(rx3 + position.x);
                int gy = (int)roundf(ry3 + position.y);
                int gz = (int)roundf(rz3 + position.z);

                // Bounds check (cast to uint so negative values also fail)
                if ((uint)gx >= WORLDSIZE || (uint)gy >= WORLDSIZE || (uint)gz >= WORLDSIZE)
                    continue;

                scene.SetVoxel(gx, gy, gz, ci);
            }
}