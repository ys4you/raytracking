#pragma once

/// @brief  A placed copy of a VoxelObject in the world, with its own transform.
///
/// Stores the raw transform parameters (position, rotation, scale, pivot)
/// AND the precomputed matrices/AABB/normals needed for ray intersection.
/// Call BuildMatrices() after changing any transform field.
struct VoxelInstance
{
    // ---- Transform parameters (user-facing) ----
    int    modelIndex;
    float3 position;
    float3 rotation;    // Euler angles in radians (Y, X, Z order)
    float3 scale;
    float3 pivot;

    // ---- Precomputed data (call BuildMatrices to refresh) ----
    mat4   localToWorld;
    mat4   worldToLocal;
    float3 worldAABBmin;
    float3 worldAABBmax;
    float3 worldNormals[6];         // +X, -X, +Y, -Y, +Z, -Z
    bool   matricesDirty = true;

    VoxelInstance() = default;

    VoxelInstance(int mIndex, float3 pos, float3 rot, float3 s, float3 p = float3(0, 0, 0))
        : modelIndex(mIndex), position(pos), rotation(rot), scale(s), pivot(p),
        matricesDirty(true)
    {
    }

    /// @brief  Recompute localToWorld, worldToLocal, world AABB, and world normals.
    ///         sizeX/Y/Z are the dimensions of the referenced VoxelObject.
    void BuildMatrices(uint sizeX, uint sizeY, uint sizeZ)
    {
        // template mat4 is row-major:
        //   row0=[0..3], row1=[4..7], row2=[8..11], row3=[12..15]
        //   translation lives in cells 3, 7, 11

        // localToWorld = T(position) * T(pivot) * Ry * Rx * Rz * S * T(-pivot)
        localToWorld = mat4::Translate(position)
            * mat4::Translate(pivot)
            * mat4::RotateY(rotation.y)
            * mat4::RotateX(rotation.x)
            * mat4::RotateZ(rotation.z)
            * mat4::Scale(scale)
            * mat4::Translate(pivot * -1.0f);

        worldToLocal = localToWorld.Inverted();

        float3 localMax = float3((float)sizeX, (float)sizeY, (float)sizeZ);
        float3 center = localMax * 0.5f;
        float3 half = center;   // localMin is (0,0,0)

        float3 wCenter = localToWorld.TransformPoint(center);

        // Row-major: row 0 = [0,1,2], row 1 = [4,5,6], row 2 = [8,9,10]
        float3 wHalf;
        wHalf.x = fabsf(localToWorld[0]) * half.x
            + fabsf(localToWorld[1]) * half.y
            + fabsf(localToWorld[2]) * half.z;
        wHalf.y = fabsf(localToWorld[4]) * half.x
            + fabsf(localToWorld[5]) * half.y
            + fabsf(localToWorld[6]) * half.z;
        wHalf.z = fabsf(localToWorld[8]) * half.x
            + fabsf(localToWorld[9]) * half.y
            + fabsf(localToWorld[10]) * half.z;

        worldAABBmin = wCenter - wHalf;
        worldAABBmax = wCenter + wHalf;

        // Pre-transform the 6 axis-aligned face normals to world space.
        // Normal transform = transpose of inverse's upper-3x3.
        // Row-major (M^-1)^T * n => read COLUMNS of worldToLocal:
        //   result.x = wTL[0]*n.x + wTL[4]*n.y + wTL[8]*n.z
        //   result.y = wTL[1]*n.x + wTL[5]*n.y + wTL[9]*n.z
        //   result.z = wTL[2]*n.x + wTL[6]*n.y + wTL[10]*n.z
        const float3 localN[6] = {
            { 1, 0, 0}, {-1, 0, 0},
            { 0, 1, 0}, { 0,-1, 0},
            { 0, 0, 1}, { 0, 0,-1}
        };
        for (int i = 0; i < 6; i++)
        {
            const float3& n = localN[i];
            float3 wn;
            wn.x = worldToLocal[0] * n.x + worldToLocal[4] * n.y + worldToLocal[8] * n.z;
            wn.y = worldToLocal[1] * n.x + worldToLocal[5] * n.y + worldToLocal[9] * n.z;
            wn.z = worldToLocal[2] * n.x + worldToLocal[6] * n.y + worldToLocal[10] * n.z;
            worldNormals[i] = normalize(wn);
        }

        matricesDirty = false;
    }
};