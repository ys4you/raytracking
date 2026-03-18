#pragma once

struct VoxelObject;
struct VoxelInstance;
struct ogt_vox_transform;

namespace Tmpl8 { class Scene; }

/// @brief  Factory for creating and placing voxel objects in the scene.
class VoxelFactory
{
public:
    static int CreateObject(Tmpl8::Scene& scene, uint sizeX, uint sizeY, uint sizeZ,
        const std::vector<uint8_t>& voxels);

    /// @brief  Create an instanced placement (TLAS path).
    ///         Automatically calls BuildMatrices on the new instance.
    static void CreateInstance(Tmpl8::Scene& scene, int objectIndex,
        float3 position, float3 rotation, float3 scale,
        float3 pivot = float3(0, 0, 0));

    /// @brief  Bake a VoxelObject into the world grid (destructive, static placement).
    static void FlattenInstance(Tmpl8::Scene& scene, int objectIndex,
        float3 position, float3 rotation, float3 scale);

    static void FromVoxTransform(const ogt_vox_transform& T,
        float3& outPos, float3& outRot, float3& outScale, float3 pivot);
};