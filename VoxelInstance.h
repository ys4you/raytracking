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
    float3 pivot;       // in LOCAL voxel units (e.g. half-size for centre rotation)

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
        // localToWorld = T(position) * Ry * Rx * Rz * S * T(-pivot)
        //
        // T(-pivot)  : shift so pivot point is at local origin
        // S          : scale from local voxel units to world units
        // Ry*Rx*Rz   : rotate in world space
        // T(position): place in world
        //
        // pivot is in LOCAL voxel units (e.g. (32,32,32) for a 64^3 object centre)
        localToWorld = mat4::Translate(position)
            * mat4::RotateY(rotation.y)
            * mat4::RotateX(rotation.x)
            * mat4::RotateZ(rotation.z)
            * mat4::Scale(scale)
            * mat4::Translate(pivot * -1.0f);

        worldToLocal = localToWorld.Inverted();

        float3 localMax = float3((float)sizeX, (float)sizeY, (float)sizeZ);

        // Transform all 8 corners of the local AABB to get tight world AABB
        float3 corners[8] = {
            float3(0, 0, 0),
            float3(localMax.x, 0, 0),
            float3(0, localMax.y, 0),
            float3(0, 0, localMax.z),
            float3(localMax.x, localMax.y, 0),
            float3(localMax.x, 0, localMax.z),
            float3(0, localMax.y, localMax.z),
            localMax
        };

        worldAABBmin = float3(1e30f);
        worldAABBmax = float3(-1e30f);
        for (int i = 0; i < 8; i++)
        {
            float3 w = localToWorld.TransformPoint(corners[i]);
            worldAABBmin = fminf(worldAABBmin, w);
            worldAABBmax = fmaxf(worldAABBmax, w);
        }

        // Pre-transform the 6 axis-aligned face normals to world space.
        // Normal transform = transpose of inverse's upper-3x3.
        // Row-major (M^-1)^T * n => read COLUMNS of worldToLocal:
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