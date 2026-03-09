#pragma once


struct VoxelInstance
{
    int modelIndex;
    float3 position;
    float3 rotation; // in radians
    float3 scale;
    float3 pivot;


    VoxelInstance() = default;

    VoxelInstance(int mIndex, float3 pos, float3 rot, float3 s, float3 p = float3(0, 0, 0))
        : modelIndex(mIndex), position(pos), rotation(rot), scale(s), pivot(p) {
    }
};