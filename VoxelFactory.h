#pragma once
#include "VoxelObject.h"
#include "VoxelInstance.h"
#include <vector>

// Forward declare instead of including
struct ogt_vox_transform;

class VoxelFactory
{
public:
    static int CreateObject(Tmpl8::Scene& scene, uint sizeX, uint sizeY, uint sizeZ, const std::vector<uint8_t>& voxels);
    static void CreateInstance(Tmpl8::Scene& scene, int objectIndex, float3 position, float3 rotation = float3(0, 0, 0), float3 scale = float3(1, 1, 1), float3 pivot = float3(0, 0, 0));
    static void FromVoxTransform(const ogt_vox_transform& T, float3& outPos, float3& outRot, float3& outScale, float3 pivot);

    static void FlattenInstance(
        Tmpl8::Scene& scene,
        int objectIndex,
        float3 position,
        float3 rotation = float3(0, 0, 0),
        float3 scale = float3(1, 1, 1)
    );
};