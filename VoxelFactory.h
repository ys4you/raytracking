#pragma once

struct VoxelObject;
struct VoxelInstance;
struct ogt_vox_transform;

namespace Tmpl8 { class Scene; }

/// <summary>Factory for creating and placing voxel objects in the scene.</summary>
class VoxelFactory
{
public:
    /// <summary>Creates a voxel object and returns its object index.</summary>
    static int CreateObject(Tmpl8::Scene& scene, uint sizeX, uint sizeY, uint sizeZ,
        const std::vector<uint8_t>& voxels);

    /// <summary>Creates an instanced placement used by the TLAS path.</summary>
    static void CreateInstance(Tmpl8::Scene& scene, int objectIndex,
        float3 position, float3 rotation, float3 scale,
        float3 pivot = float3(0, 0, 0));

    /// <summary>Bakes a voxel object into the world grid as static geometry.</summary>
    static void FlattenInstance(Tmpl8::Scene& scene, int objectIndex,
        float3 position, float3 rotation, float3 scale);

    /// <summary>Converts a VOX transform into engine transform components.</summary>
    static void FromVoxTransform(const ogt_vox_transform& T,
        float3& outPos, float3& outRot, float3& outScale, float3 pivot);
};
