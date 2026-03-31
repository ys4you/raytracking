#pragma once
#include "Sphere.h"
#include "tiny_bvh.h"

#define WORLDSIZE 512
#define GRIDSIZE  WORLDSIZE
#define GRIDSIZE2 WORLDSIZE * WORLDSIZE
#define GRIDSIZE3 WORLDSIZE * WORLDSIZE * WORLDSIZE
#define WORLDSIZE2 (WORLDSIZE*WORLDSIZE)
#define WORLDSIZE3 (WORLDSIZE*WORLDSIZE*WORLDSIZE)
#define BRICK_SIZE 16
#define BRICK_SIZE3 (BRICK_SIZE*BRICK_SIZE*BRICK_SIZE)
#define BRICK_SHIFT 4
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
    MAT_LAMBERTIAN_WHITE = 4,


    MAT_RANDOM_START = 5,
    MAT_RANDOM_END = 105,
    MAT_COUNT = 106
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

        /// <summary>Returns the voxel value at world coordinates.</summary>
        uint GetVoxel(uint x, uint y, uint z) const;
        /// <summary>Allocates a new brick for sparse voxel storage.</summary>
        uint AllocateBrick();

        /// <summary>Constructs an empty scene with default storage.</summary>
        Scene();

        /// <summary>Finds the nearest intersection for a ray.</summary>
        void FindNearest(Ray& ray) const;
        /// <summary>Returns true when any geometry occludes the ray.</summary>
        bool IsOccluded(Ray& ray) const;
        /// <summary>Sets a world voxel value at integer coordinates.</summary>
        void Set(const uint x, const uint y, const uint z, const uint v);
        /// <summary>Sets a world voxel with bounds-safe integer coordinates.</summary>
        void SetVoxel(int x, int y, int z, uint materialIndex);
        /// <summary>Builds the sphere BVH acceleration structure.</summary>
        void BuildSphereBVH();

        /// <summary>Rebuilds matrices for dirty voxel instances before rendering.</summary>
        void RebuildDirtyInstances();

        /// <summary>Initializes DDA traversal state for a ray.</summary>
        bool Setup3DDDA(Ray& ray, DDAState& state) const;

        /// <summary>Returns material data for a voxel palette value.</summary>
        [[nodiscard]] const Material& GetMat(uint voxelValue) const
        {
            return materials[MAT_COUNT + (voxelValue - 1)];
        }
        /// <summary>Returns material data for a sphere material identifier.</summary>
        [[nodiscard]] const Material& GetSphereMat(uint matID) const
        {
            return materials[matID];
        }

        /// <summary>Clears the world voxel grid and occupancy map.</summary>
        void ClearWorld()
        {
            memset(coarseGrid, 0, COARSE_SIZE3 * sizeof(uint));
            memset(occupancy, 0, sizeof(occupancy));

            for (auto* b : bricks)
                memset(b, 0, BRICK_SIZE3);
        }

        bool voxelGridActive = true;

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

        std::vector<VoxelObject>   voxelObjects;
        std::vector<VoxelInstance> voxelInstances;

        bool instancesShadows = true;


        tinybvh::BVH              instanceBVH;
        bool                      instanceBVHReady = false;
        std::vector<int>          instancePrimToSceneIndex;


        /// <summary>Rebuilds the TLAS BVH over voxel instances.</summary>
        void BuildInstanceBVH();


        /// <summary>Traces voxel instances through the instance BVH.</summary>
        void TraceInstanceBVH(Ray& ray, float& bestT, int& bestInstanceIdx,
            int& bestAxis, uint& bestVoxel) const;
        /// <summary>Tests occlusion against voxel instances through the BVH.</summary>
        bool TraceInstanceBVHOcclusion(Ray& ray) const;

        static const VoxelInstance* g_instances;


    private:
        void TraceSphereGrid(Ray& ray, float& nearestT, int& nearestIdx) const;
        void TraceSphereBVH(tinybvh::Ray& ray) const;
        __forceinline void IntersectSphereInlined(tinybvh::Ray& ray, uint32_t idx) const;

        template<bool IsOcclusionRay>
        bool TraverseDDA(Ray& ray, float nearestSphereT, uint& outMaterial, int& outAxis) const;

        float TraceObjectDDA(const float3& localO, const float3& localD,
            const VoxelObject& obj, float tMax,
            int& outFace, uint8_t& outVoxel) const;

        bool TraceObjectDDAOcclusion(const float3& localO, const float3& localD,
            const VoxelObject& obj, float tMax) const;

        /// <summary>Performs a slab-based ray versus AABB intersection test.</summary>
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
