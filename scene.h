#pragma once
#include "Sphere.h"
#include "tiny_bvh.h"

#define WORLDSIZE 256
#define GRIDSIZE  WORLDSIZE
#define GRIDSIZE2 WORLDSIZE * WORLDSIZE
#define GRIDSIZE3 WORLDSIZE * WORLDSIZE * WORLDSIZE
#define WORLDSIZE2 (WORLDSIZE*WORLDSIZE)
#define WORLDSIZE3 (WORLDSIZE*WORLDSIZE*WORLDSIZE)
#define BRICK_SIZE 8
#define BRICK_SIZE3 (BRICK_SIZE*BRICK_SIZE*BRICK_SIZE)
#define BRICK_SHIFT 3
#define BRICK_SIZE2 (BRICK_SIZE*BRICK_SIZE)
#define COARSE_SIZE (WORLDSIZE / BRICK_SIZE)
#define COARSE_SIZE2 (COARSE_SIZE*COARSE_SIZE)
#define COARSE_SIZE3 (COARSE_SIZE*COARSE_SIZE*COARSE_SIZE)
#define EPSILON 0.00001f

#include "VoxelInstance.h"
#include "VoxelObject.h"

enum MaterialID : uint
{
    MAT_NONE = 0,
    MAT_MIRROR = 1,
    MAT_DIELECTRIC = 2,
    MAT_GREEN = 3,
    MAT_COUNT = 4
};
static constexpr int TOTAL_MATS = MAT_COUNT + 256;

struct Material;

namespace Tmpl8
{
    class Scene
    {
    public:
        struct DDAState
        {
            int3 step;
            uint X, Y, Z;
            float t;
            float3 tdelta;
            float3 tmax;
            int axis;
        };

        uint GetVoxel(uint x, uint y, uint z) const;
        uint AllocateBrick();
        Scene();

        void FindNearest(Ray& ray) const;
        bool IsOccluded(Ray& ray) const;
        void Set(const uint x, const uint y, const uint z, const uint v);
        void SetVoxel(int x, int y, int z, uint materialIndex);
        void BuildSphereBVH();

        uint* coarseGrid;
        std::vector<uint8_t*> bricks;
        std::array<Material, MAT_COUNT + 256> materials;
        std::vector<Sphere> spheres;
        tinybvh::BVH sphereBVH;
        bool sphereBVHReady = false;

        // Accessible by free callbacks
        static const Sphere* g_spheres;

        const Material& GetMat(uint voxelValue) const
        {
            return materials[MAT_COUNT + (voxelValue - 1)];
        }
        const Material& GetSphereMat(uint matID) const
        {
            return materials[matID];
        }

	    std::vector<VoxelObject> voxelObjects;
	    std::vector<VoxelInstance> voxelInstances;

    private:
        bool Setup3DDDA(Ray& ray, DDAState& state) const;
    };
}