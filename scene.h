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
    MAT_RANDOM_END = 104,
    MAT_COUNT = 105
};
static constexpr int TOTAL_MATS = MAT_COUNT + 256;

struct Material;

struct SphereSOA
{
    std::vector<float> cx, cy, cz, r2, radius;
    std::vector<int>   material;
    uint32_t count = 0;

    void Rebuild(const std::vector<Sphere>& spheres)
    {
        count = static_cast<uint32_t>(spheres.size());
        const uint32_t padded = (count + 7u) & ~7u;
        cx.assign(padded, 0.f); cy.assign(padded, 0.f); cz.assign(padded, 0.f);
        r2.assign(padded, -1.f); radius.assign(padded, 0.f); material.assign(padded, 0);
        for (uint32_t i = 0; i < count; ++i)
        {
            cx[i] = spheres[i].center.x; cy[i] = spheres[i].center.y; cz[i] = spheres[i].center.z;
            r2[i] = spheres[i].radius * spheres[i].radius;
            radius[i] = spheres[i].radius;
            material[i] = static_cast<int>(spheres[i].material);
        }
    }
};

struct SphereGrid
{
    static constexpr int SGRID_RES = 16;
    static constexpr int SGRID_RES2 = SGRID_RES * SGRID_RES;
    static constexpr int SGRID_RES3 = SGRID_RES * SGRID_RES * SGRID_RES;
    static constexpr float CELL_SIZE = 1.0f / SGRID_RES;

    std::vector<uint32_t> indices;
    uint32_t cellStart[SGRID_RES3];
    uint32_t cellCount[SGRID_RES3];
    float3 boundsMin, boundsMax;
    bool ready = false;

    void Build(const std::vector<Sphere>& spheres)
    {
        ready = false;
        if (spheres.empty()) return;
        memset(cellCount, 0, sizeof(cellCount));
        boundsMin = float3(1e30f); boundsMax = float3(-1e30f);
        for (const auto& s : spheres)
        {
            boundsMin = fminf(boundsMin, s.center - float3(s.radius));
            boundsMax = fmaxf(boundsMax, s.center + float3(s.radius));
        }
        boundsMin -= float3(0.001f); boundsMax += float3(0.001f);
        for (uint32_t i = 0; i < (uint32_t)spheres.size(); i++)
        {
            const Sphere& s = spheres[i];
            int x0 = max(0, (int)((s.center.x - s.radius) * SGRID_RES));
            int y0 = max(0, (int)((s.center.y - s.radius) * SGRID_RES));
            int z0 = max(0, (int)((s.center.z - s.radius) * SGRID_RES));
            int x1 = min(SGRID_RES - 1, (int)((s.center.x + s.radius) * SGRID_RES));
            int y1 = min(SGRID_RES - 1, (int)((s.center.y + s.radius) * SGRID_RES));
            int z1 = min(SGRID_RES - 1, (int)((s.center.z + s.radius) * SGRID_RES));
            for (int z = z0; z <= z1; z++)
                for (int y = y0; y <= y1; y++)
                    for (int x = x0; x <= x1; x++)
                        cellCount[x + y * SGRID_RES + z * SGRID_RES2]++;
        }
        uint32_t total = 0;
        for (int c = 0; c < SGRID_RES3; c++) { cellStart[c] = total; total += cellCount[c]; }
        indices.resize(total);
        uint32_t cursor[SGRID_RES3];
        memcpy(cursor, cellStart, sizeof(cellStart));
        for (uint32_t i = 0; i < (uint32_t)spheres.size(); i++)
        {
            const Sphere& s = spheres[i];
            int x0 = max(0, (int)((s.center.x - s.radius) * SGRID_RES));
            int y0 = max(0, (int)((s.center.y - s.radius) * SGRID_RES));
            int z0 = max(0, (int)((s.center.z - s.radius) * SGRID_RES));
            int x1 = min(SGRID_RES - 1, (int)((s.center.x + s.radius) * SGRID_RES));
            int y1 = min(SGRID_RES - 1, (int)((s.center.y + s.radius) * SGRID_RES));
            int z1 = min(SGRID_RES - 1, (int)((s.center.z + s.radius) * SGRID_RES));
            for (int z = z0; z <= z1; z++)
                for (int y = y0; y <= y1; y++)
                    for (int x = x0; x <= x1; x++)
                    {
                        uint32_t c = x + y * SGRID_RES + z * SGRID_RES2; indices[cursor[c]++] = i;
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

        /// @brief  Rebuild matrices for any dirty voxel instances.
        ///         Call once per frame at the start of Tick, before the render loop.
        void RebuildDirtyInstances();

        bool Setup3DDDA(Ray& ray, DDAState& state) const;

        [[nodiscard]] const Material& GetMat(uint voxelValue) const
        {
            return materials[MAT_COUNT + (voxelValue - 1)];
        }
        [[nodiscard]] const Material& GetSphereMat(uint matID) const
        {
            return materials[matID];
        }

        // ---- Data ----
        uint* coarseGrid;
        std::vector<uint8_t*>                 bricks;
        std::array<Material, MAT_COUNT + 256> materials;

        uint32_t occupancy[COARSE_SIZE3 / 32 + 1] = {};
        __forceinline void SetOccupied(uint32_t idx) { occupancy[idx >> 5] |= (1u << (idx & 31)); }
        __forceinline void ClearOccupied(uint32_t idx) { occupancy[idx >> 5] &= ~(1u << (idx & 31)); }
        __forceinline bool IsOccupied(uint32_t idx) const { return (occupancy[idx >> 5] >> (idx & 31)) & 1u; }
        __forceinline bool IsWordEmpty(uint32_t idx) const { return occupancy[idx >> 5] == 0; }

        std::vector<Sphere>   spheres;
        SphereSOA             sphereSOA;
        SphereGrid            sphereGrid;
        tinybvh::BVH          sphereBVH;
        bool                  sphereBVHReady = false;
        bool                  useLegacyBVH = false;
        static const Sphere* g_spheres;

        // Voxel instancing (TLAS/BLAS)
        std::vector<VoxelObject>   voxelObjects;
        std::vector<VoxelInstance> voxelInstances;

    private:
        void TraceSphereGrid(Ray& ray, float& nearestT, int& nearestIdx) const;
        void TraceSphereBVH(tinybvh::Ray& ray) const;
        __forceinline void IntersectSphereInlined(tinybvh::Ray& ray, uint32_t idx) const;

        template<bool IsOcclusionRay>
        bool TraverseDDA(Ray& ray, float nearestSphereT, uint& outMaterial, int& outAxis) const;

        // ---- Per-object DDA for instanced voxel objects ----
        float TraceObjectDDA(const float3& localO, const float3& localD,
            const VoxelObject& obj, float tMax,
            int& outFace, uint8_t& outVoxel) const;

        bool TraceObjectDDA_Occlusion(const float3& localO, const float3& localD,
            const VoxelObject& obj, float tMax) const;

        /// @brief  Ray vs AABB slab test.
        static __forceinline bool RayAABB(const float3& O, const float3& rD,
            const float3& bmin, const float3& bmax,
            float& tEntry, float& tExit)
        {
            const float tx1 = (bmin.x - O.x) * rD.x, tx2 = (bmax.x - O.x) * rD.x;
            const float ty1 = (bmin.y - O.y) * rD.y, ty2 = (bmax.y - O.y) * rD.y;
            const float tz1 = (bmin.z - O.z) * rD.z, tz2 = (bmax.z - O.z) * rD.z;
            tEntry = max(max(min(tx1, tx2), min(ty1, ty2)), min(tz1, tz2));
            tExit = min(min(max(tx1, tx2), max(ty1, ty2)), max(tz1, tz2));
            return tExit >= tEntry && tExit >= 0.f;
        }
    };
}