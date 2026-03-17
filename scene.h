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

    MAT_RANDOM_START = 4,
    MAT_RANDOM_END = 104,   // 100 random materials
    MAT_COUNT = 105
};
static constexpr int TOTAL_MATS = MAT_COUNT + 256;

struct Material;

// ---------------------------------------------------------------------------
// SphereSOA Structure-of-Arrays layout for sphere data.
//
// Keeping each component in its own contiguous array lets AVX2 load 8 floats
// at once (256-bit registers), so IntersectSphereInlined can test 8 spheres
// per SIMD iteration instead of one.
//
// Lifecycle: rebuilt by Scene::BuildSphereBVH() from the AoS spheres vector.
// The AoS vector remains the authoritative source for spawning and BVH build;
// SOA is a derived, read-only hot-path structure.
// ---------------------------------------------------------------------------
struct SphereSOA
{
    // Each vector is padded to the next multiple of 8 with sentinel values
    // (radius = -1) so the AVX loop never reads uninitialised memory.
    std::vector<float> cx;
    std::vector<float> cy;
    std::vector<float> cz;
    std::vector<float> r2;        // radius squared — avoids a multiply per test
    std::vector<float> radius;    // raw radius, kept for normal reconstruction
    std::vector<int>   material;

    uint32_t count = 0;   // actual sphere count (unpadded)

    void Rebuild(const std::vector<Sphere>& spheres)
    {
        count = (uint32_t)spheres.size();

        // Round up to the next multiple of 8 for the AVX8 brute-force path.
        const uint32_t padded = (count + 7u) & ~7u;

        cx.assign(padded, 0.f);
        cy.assign(padded, 0.f);
        cz.assign(padded, 0.f);
        r2.assign(padded, -1.f);   // sentinel: discriminant always < 0
        radius.assign(padded, 0.f);
        material.assign(padded, 0);

        for (uint32_t i = 0; i < count; ++i)
        {
            cx[i] = spheres[i].center.x;
            cy[i] = spheres[i].center.y;
            cz[i] = spheres[i].center.z;
            r2[i] = spheres[i].radius * spheres[i].radius;
            radius[i] = spheres[i].radius;
            material[i] = static_cast<int>(spheres[i].material);
        }
    }
};

namespace Tmpl8
{
    class Scene
    {
    public:
        struct DDAState
        {
            int3   step;
            uint   X, Y, Z;
            float  t;
            float3 tdelta;
            float3 tmax;
            int    axis;
        };

        uint GetVoxel(uint x, uint y, uint z) const;
        uint AllocateBrick();

        Scene();

        void FindNearest(Ray& ray) const;
        bool IsOccluded(Ray& ray) const;
        void Set(const uint x, const uint y, const uint z, const uint v);
        void SetVoxel(int x, int y, int z, uint materialIndex);
        void BuildSphereBVH();

        // Public so external callers (e.g. physics) can initialise DDA state.
        bool Setup3DDDA(Ray& ray, DDAState& state) const;

        [[nodiscard]] const Material& GetMat(uint voxelValue) const
        {
            return materials[MAT_COUNT + (voxelValue - 1)];
        }

        [[nodiscard]] const Material& GetSphereMat(uint matID) const
        {
            return materials[matID];
        }

        uint* coarseGrid;
        std::vector<uint8_t*>                     bricks;
        std::array<Material, MAT_COUNT + 256>     materials;

        // -----------------------------------------------------------------
        // Coarse-grid occupancy bitmap (credit: Thomas)
        //
        // 1-bit per coarse cell: set when the cell has a brick.
        // For a 32^3 coarse grid this is ~4 KB — fits comfortably in L1
        // cache, letting the DDA skip empty bricks without touching the
        // full coarseGrid array (~128 KB, cache-unfriendly).
        // -----------------------------------------------------------------
        uint32_t occupancy[COARSE_SIZE3 / 32 + 1] = {};

        // credit: Thomas — occupancy bitmap helpers
        __forceinline void SetOccupied(uint32_t idx)
        {
            occupancy[idx >> 5] |= (1u << (idx & 31));
        }
        __forceinline void ClearOccupied(uint32_t idx)
        {
            occupancy[idx >> 5] &= ~(1u << (idx & 31));
        }
        __forceinline bool IsOccupied(uint32_t idx) const
        {
            return (occupancy[idx >> 5] >> (idx & 31)) & 1u;
        }
        // Returns true if the entire 32-cell word containing idx is empty.
        // Used to skip whole runs of empty coarse cells in one branch.
        __forceinline bool IsWordEmpty(uint32_t idx) const
        {
            return occupancy[idx >> 5] == 0;
        }

        // AoS — authoritative source; used for spawning and BVH construction.
        std::vector<Sphere>   spheres;

        // SOA — derived from spheres in BuildSphereBVH(); used by the hot path.
        SphereSOA             sphereSOA;

        tinybvh::BVH          sphereBVH;
        bool                  sphereBVHReady = false;

        static const Sphere* g_spheres;

        std::vector<VoxelObject>   voxelObjects;
        std::vector<VoxelInstance> voxelInstances;

    private:
        void TraceSphereBVH(tinybvh::Ray& ray) const;
        __forceinline void IntersectSphereInlined(tinybvh::Ray& ray, uint32_t idx) const;

        template<bool IsOcclusionRay>
        bool TraverseDDA(Ray& ray, float nearestSphereT,
            uint& outMaterial, int& outAxis) const;
    };
}