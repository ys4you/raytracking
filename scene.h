#pragma once
#include "Sphere.h"
#include "tiny_bvh.h"

#define WORLDSIZE /*8*/ 256
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
    MAT_RANDOM_END = 104,
    MAT_COUNT = 105
};
static constexpr int TOTAL_MATS = MAT_COUNT + 256;

struct Material;

struct SphereSOA
{
    std::vector<float> cx;
    std::vector<float> cy;
    std::vector<float> cz;
    std::vector<float> r2;      // radius squared
    std::vector<float> radius;
    std::vector<int>   material;

    uint32_t count = 0;

    void Rebuild(const std::vector<Sphere>& spheres)
    {
        count = static_cast<uint32_t>(spheres.size());
        const uint32_t padded = (count + 7u) & ~7u;

        cx.assign(padded, 0.f);
        cy.assign(padded, 0.f);
        cz.assign(padded, 0.f);
        r2.assign(padded, -1.f);
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

/// Uniform grid over [0,1]^3 for sphere acceleration.
/// Cells store packed lists of sphere indices. Traversal is DDA front-to-back.
struct SphereGrid
{
    static constexpr int SGRID_RES = 16;
    static constexpr int SGRID_RES2 = SGRID_RES * SGRID_RES;
    static constexpr int SGRID_RES3 = SGRID_RES * SGRID_RES * SGRID_RES;
    static constexpr float CELL_SIZE = 1.0f / SGRID_RES;

    std::vector<uint32_t> indices;  // all sphere indices, packed per cell
    uint32_t cellStart[SGRID_RES3];
    uint32_t cellCount[SGRID_RES3];

    float3 boundsMin, boundsMax;    // AABB of all spheres
    bool ready = false;

    void Build(const std::vector<Sphere>& spheres)
    {
        ready = false;
        if (spheres.empty()) return;

        memset(cellCount, 0, sizeof(cellCount));

        boundsMin = float3(1e30f);
        boundsMax = float3(-1e30f);
        for (const auto& s : spheres)
        {
            boundsMin = fminf(boundsMin, s.center - float3(s.radius));
            boundsMax = fmaxf(boundsMax, s.center + float3(s.radius));
        }
        boundsMin -= float3(0.001f);
        boundsMax += float3(0.001f);

        // Count how many spheres land in each cell
        for (uint32_t i = 0; i < static_cast<uint32_t>(spheres.size()); i++)
        {
            const Sphere& s = spheres[i];
            int x0 = max(0, static_cast<int>((s.center.x - s.radius) * SGRID_RES));
            int y0 = max(0, static_cast<int>((s.center.y - s.radius) * SGRID_RES));
            int z0 = max(0, static_cast<int>((s.center.z - s.radius) * SGRID_RES));
            int x1 = min(SGRID_RES - 1, static_cast<int>((s.center.x + s.radius) * SGRID_RES));
            int y1 = min(SGRID_RES - 1, static_cast<int>((s.center.y + s.radius) * SGRID_RES));
            int z1 = min(SGRID_RES - 1, static_cast<int>((s.center.z + s.radius) * SGRID_RES));

            for (int z = z0; z <= z1; z++)
                for (int y = y0; y <= y1; y++)
                    for (int x = x0; x <= x1; x++)
                        cellCount[x + y * SGRID_RES + z * SGRID_RES2]++;
        }

        // Prefix sum to get cell offsets
        uint32_t total = 0;
        for (int c = 0; c < SGRID_RES3; c++)
        {
            cellStart[c] = total;
            total += cellCount[c];
        }
        indices.resize(total);

        // Write sphere indices into their cells
        uint32_t cursor[SGRID_RES3];
        memcpy(cursor, cellStart, sizeof(cellStart));

        for (uint32_t i = 0; i < static_cast<uint32_t>(spheres.size()); i++)
        {
            const Sphere& s = spheres[i];
            int x0 = max(0, static_cast<int>((s.center.x - s.radius) * SGRID_RES));
            int y0 = max(0, static_cast<int>((s.center.y - s.radius) * SGRID_RES));
            int z0 = max(0, static_cast<int>((s.center.z - s.radius) * SGRID_RES));
            int x1 = min(SGRID_RES - 1, static_cast<int>((s.center.x + s.radius) * SGRID_RES));
            int y1 = min(SGRID_RES - 1, static_cast<int>((s.center.y + s.radius) * SGRID_RES));
            int z1 = min(SGRID_RES - 1, static_cast<int>((s.center.z + s.radius) * SGRID_RES));

            for (int z = z0; z <= z1; z++)
                for (int y = y0; y <= y1; y++)
                    for (int x = x0; x <= x1; x++)
                    {
                        uint32_t c = x + y * SGRID_RES + z * SGRID_RES2;
                        indices[cursor[c]++] = i;
                    }
        }

        ready = true;
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

        // Coarse-grid occupancy bitmap (credit: Thomas)
        uint32_t occupancy[COARSE_SIZE3 / 32 + 1] = {};

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
        __forceinline bool IsWordEmpty(uint32_t idx) const
        {
            return occupancy[idx >> 5] == 0;
        }

        std::vector<Sphere>   spheres;
        SphereSOA             sphereSOA;
        SphereGrid            sphereGrid;

        tinybvh::BVH          sphereBVH;        // legacy fallback
        bool                  sphereBVHReady = false;
        bool                  useLegacyBVH = false;  // toggle for A/B comparison
        static const Sphere* g_spheres;

        std::vector<VoxelObject>   voxelObjects;
        std::vector<VoxelInstance> voxelInstances;

    private:
        void TraceSphereGrid(Ray& ray, float& nearestT, int& nearestIdx) const;

        void TraceSphereBVH(tinybvh::Ray& ray) const;
        __forceinline void IntersectSphereInlined(tinybvh::Ray& ray, uint32_t idx) const;

        template<bool IsOcclusionRay>
        bool TraverseDDA(Ray& ray, float nearestSphereT,
            uint& outMaterial, int& outAxis) const;
    };
}