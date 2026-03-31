#pragma once
/// <summary>Represents a placed voxel object with transform and cached data.</summary>
struct VoxelInstance
{
    int    modelIndex;
    float3 position;
    float3 rotation;
    float3 scale;
    float3 pivot;

    mat4   localToWorld;
    mat4   worldToLocal;
    float3 worldAABBmin;
    float3 worldAABBmax;
    float3 worldNormals[6];
    bool   matricesDirty = true;

    VoxelInstance() = default;
    VoxelInstance(int mIndex, float3 pos, float3 rot, float3 s, float3 p = float3(0, 0, 0))
        : modelIndex(mIndex), position(pos), rotation(rot), scale(s), pivot(p),
        matricesDirty(true)
    {
    }

    /// <summary>Recomputes transform matrices, world AABB, and world normals.</summary>
    void BuildMatrices(uint sizeX, uint sizeY, uint sizeZ)
    {
        localToWorld = mat4::Translate(position)
            * mat4::RotateY(rotation.y)
            * mat4::RotateX(rotation.x)
            * mat4::RotateZ(rotation.z)
            * mat4::Scale(scale)
            * mat4::Translate(pivot * -1.0f);

        worldToLocal = localToWorld.Inverted();

        float3 localMax = float3((float)sizeX, (float)sizeY, (float)sizeZ);

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
