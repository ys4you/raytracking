#include "template.h"
#include "VoxLoader.h"
#include "Core/Material.h"
#include "Sphere.h"
#include "VoxelFactory.h"

#include <random>
#include <immintrin.h>   // AVX2

const Sphere* Scene::g_spheres = nullptr;

// ---------------------------------------------------------------------------
// SphereAABB — tinybvh build callback.  Still reads from the AoS g_spheres
// array; only the per-ray intersection hot path uses the SOA.
// ---------------------------------------------------------------------------
static void SphereAABB(uint32_t idx, tinybvh::bvhvec3& mn, tinybvh::bvhvec3& mx)
{
    const Sphere& s = Scene::g_spheres[idx];
    mn = { s.center.x - s.radius, s.center.y - s.radius, s.center.z - s.radius };
    mx = { s.center.x + s.radius, s.center.y + s.radius, s.center.z + s.radius };
}

// ---------------------------------------------------------------------------
// TraceSphereBVH — hand-rolled ordered-stack traversal.
//
// Calls IntersectSphereInlined directly at every leaf, eliminating
// the customIntersect function-pointer indirection that sphereBVH.Intersect()
// would otherwise use. The compiler can inline the sphere math directly
// into the leaf loop body, giving a measurable win over the callback path.
// ---------------------------------------------------------------------------
void Scene::TraceSphereBVH(tinybvh::Ray& ray) const
{
    using Node = tinybvh::BVH::BVHNode;

    const Node* node = &sphereBVH.bvhNode[0];
    const Node* stack[64];
    uint32_t    stackPtr = 0;

    while (true)
    {
        if (node->isLeaf())
        {
            for (uint32_t i = 0; i < node->triCount; ++i)
                IntersectSphereInlined(ray, sphereBVH.primIdx[node->leftFirst + i]);

            if (stackPtr == 0) break;
            node = stack[--stackPtr];
            continue;
        }

        const Node* child1 = &sphereBVH.bvhNode[node->leftFirst];
        const Node* child2 = &sphereBVH.bvhNode[node->leftFirst + 1];

        float dist1 = tinybvh::tinybvh_intersect_aabb(ray, child1->aabbMin, child1->aabbMax);
        float dist2 = tinybvh::tinybvh_intersect_aabb(ray, child2->aabbMin, child2->aabbMax);

        if (dist1 > dist2)
        {
            tinybvh::tinybvh_swap(dist1, dist2);
            tinybvh::tinybvh_swap(child1, child2);
        }

        if (dist1 == BVH_FAR)
        {
            if (stackPtr == 0) break;
            node = stack[--stackPtr];
        }
        else
        {
            node = child1;
            if (dist2 != BVH_FAR)
                stack[stackPtr++] = child2;
        }
    }
}

// ---------------------------------------------------------------------------
// IntersectSphereInlined — reads from SphereSOA, force-inlined into leaf loop.
// ---------------------------------------------------------------------------
__forceinline void Scene::IntersectSphereInlined(tinybvh::Ray& ray, uint32_t idx) const
{
    const float ocx = ray.O.x - sphereSOA.cx[idx];
    const float ocy = ray.O.y - sphereSOA.cy[idx];
    const float ocz = ray.O.z - sphereSOA.cz[idx];

    const float b = ocx * ray.D.x + ocy * ray.D.y + ocz * ray.D.z;
    const float oc2 = ocx * ocx + ocy * ocy + ocz * ocz;
    const float disc = b * b - (oc2 - sphereSOA.r2[idx]);

    if (disc <= 0.f) return;

    const float sqrtDisc = sqrtf(disc);
    float t = -b - sqrtDisc;
    if (t <= 0.f) t = -b + sqrtDisc;

    if (t > 0.f && t < ray.hit.t)
    {
        ray.hit.t = t;
        ray.hit.prim = idx;
    }
}

// ---------------------------------------------------------------------------
// IntersectSpheresAVX8 — brute-force 8-wide AVX2 intersection.
//
// Used when sphereBVHReady is false (< 2 spheres, or mid-rebuild).
// Processes 8 spheres per iteration using 256-bit SIMD.  The SOA padding
// guarantees the arrays are always a multiple-of-8 in length, so no
// tail-handling is needed.
// ---LLM helped---
// Returns the index of the nearest hit, or -1 on miss.
// ---------------------------------------------------------------------------
static int IntersectSpheresAVX8(
    const SphereSOA& soa,
    const tinybvh::Ray& ray,
    float& outT)
{
    const __m256 ox = _mm256_set1_ps(ray.O.x);
    const __m256 oy = _mm256_set1_ps(ray.O.y);
    const __m256 oz = _mm256_set1_ps(ray.O.z);
    const __m256 dx = _mm256_set1_ps(ray.D.x);
    const __m256 dy = _mm256_set1_ps(ray.D.y);
    const __m256 dz = _mm256_set1_ps(ray.D.z);

    __m256 bestT = _mm256_set1_ps(outT);
    __m256 bestIdx = _mm256_set1_ps(-1.f);

    const uint32_t padded = (uint32_t)soa.cx.size();

    for (uint32_t i = 0; i < padded; i += 8)
    {
        // oc = ray.O - sphere.center
        const __m256 ocx = _mm256_sub_ps(ox, _mm256_loadu_ps(soa.cx.data() + i));
        const __m256 ocy = _mm256_sub_ps(oy, _mm256_loadu_ps(soa.cy.data() + i));
        const __m256 ocz = _mm256_sub_ps(oz, _mm256_loadu_ps(soa.cz.data() + i));

        // b = dot(oc, D)
        __m256 b = _mm256_fmadd_ps(ocx, dx,
            _mm256_fmadd_ps(ocy, dy,
                _mm256_mul_ps(ocz, dz)));

        // oc2 = dot(oc, oc)
        __m256 oc2 = _mm256_fmadd_ps(ocx, ocx,
            _mm256_fmadd_ps(ocy, ocy,
                _mm256_mul_ps(ocz, ocz)));

        // disc = b*b - (oc2 - r2)
        const __m256 r2 = _mm256_loadu_ps(soa.r2.data() + i);
        const __m256 disc = _mm256_sub_ps(
            _mm256_mul_ps(b, b),
            _mm256_sub_ps(oc2, r2));

        // skip lanes where disc <= 0
        const __m256 zero = _mm256_setzero_ps();
        const __m256 valid = _mm256_cmp_ps(disc, zero, _CMP_GT_OQ);
        if (_mm256_movemask_ps(valid) == 0) continue;

        const __m256 sqrtDisc = _mm256_sqrt_ps(
            _mm256_max_ps(disc, zero));   // clamp for safety

        // t0 = -b - sqrt(disc),  t1 = -b + sqrt(disc)
        const __m256 nb = _mm256_sub_ps(zero, b);
        __m256 t0 = _mm256_sub_ps(nb, sqrtDisc);
        __m256 t1 = _mm256_add_ps(nb, sqrtDisc);

        // pick t0 if t0 > 0, else t1
        __m256 useT0 = _mm256_cmp_ps(t0, zero, _CMP_GT_OQ);
        __m256 t = _mm256_blendv_ps(t1, t0, useT0);

        // keep only t > 0 and t < bestT
        __m256 mask = _mm256_and_ps(valid,
            _mm256_and_ps(
                _mm256_cmp_ps(t, zero, _CMP_GT_OQ),
                _mm256_cmp_ps(t, bestT, _CMP_LT_OQ)));

        if (_mm256_movemask_ps(mask) == 0) continue;

        // update bestT lane-by-lane
        bestT = _mm256_blendv_ps(bestT, t, mask);

        // record lane indices as floats
        const __m256 idx8 = _mm256_set_ps(
            float(i + 7), float(i + 6), float(i + 5), float(i + 4),
            float(i + 3), float(i + 2), float(i + 1), float(i + 0));
        bestIdx = _mm256_blendv_ps(bestIdx, idx8, mask);
    }

    // horizontal reduce: find the lane with the minimum t
    // Use a simple scalar reduce over the 8 lanes.
    alignas(32) float tArr[8];
    alignas(32) float iArr[8];
    _mm256_store_ps(tArr, bestT);
    _mm256_store_ps(iArr, bestIdx);

    float minT = outT;
    int   minI = -1;
    for (int k = 0; k < 8; ++k)
    {
        if (iArr[k] >= 0.f && tArr[k] < minT)
        {
            minT = tArr[k];
            minI = (int)iArr[k];
        }
    }
    outT = minT;
    return minI;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

__forceinline static float intersect_cube(Ray& ray)
{
    const float tx1 = -ray.O.x * ray.rD.x, tx2 = (1.f - ray.O.x) * ray.rD.x;
    const float ty1 = -ray.O.y * ray.rD.y, ty2 = (1.f - ray.O.y) * ray.rD.y;
    const float tz1 = -ray.O.z * ray.rD.z, tz2 = (1.f - ray.O.z) * ray.rD.z;

    const float ty = min(ty1, ty2);
    const float tz = min(tz1, tz2);
    float tmin = max(max(min(tx1, tx2), ty), tz);
    float tmax = min(min(max(tx1, tx2), max(ty1, ty2)), max(tz1, tz2));

    if (tmin == tz) ray.axis = 2;
    else if (tmin == ty) ray.axis = 1;

    return tmax >= tmin ? tmin : 1e34f;
}

__forceinline static bool point_in_cube(const float3& pos)
{
    return pos.x >= 0.f && pos.y >= 0.f && pos.z >= 0.f
        && pos.x <= 1.f && pos.y <= 1.f && pos.z <= 1.f;
}

// ---------------------------------------------------------------------------
// Brick grid
// ---------------------------------------------------------------------------

uint Scene::GetVoxel(uint x, uint y, uint z) const
{
    const uint gidx = (x / BRICK_SIZE)
        + (y / BRICK_SIZE) * COARSE_SIZE
        + (z / BRICK_SIZE) * COARSE_SIZE2;
    const uint cell = coarseGrid[gidx];

    if (cell == 0)        return 0;
    if ((cell & 1) == 0)  return cell >> 1;

    const uint lx = x & (BRICK_SIZE - 1);
    const uint ly = y & (BRICK_SIZE - 1);
    const uint lz = z & (BRICK_SIZE - 1);

    return bricks[cell >> 1][lx + ly * BRICK_SIZE + lz * BRICK_SIZE2];
}

uint Scene::AllocateBrick()
{
    uint8_t* b = (uint8_t*)MALLOC64(BRICK_SIZE3);
    memset(b, 0, BRICK_SIZE3);
    bricks.push_back(b);
    return (uint)bricks.size() - 1;
}

// ---------------------------------------------------------------------------
// Scene constructor
// ---------------------------------------------------------------------------
Scene::Scene()
{
    coarseGrid = (uint*)MALLOC64(COARSE_SIZE3 * sizeof(uint));
    memset(coarseGrid, 0, COARSE_SIZE3 * sizeof(uint));

    // credit: Thomas — zero out occupancy bitmap
    memset(occupancy, 0, sizeof(occupancy));

    bricks.clear();

    materials.fill(Material{});

    materials[MAT_MIRROR].type = MaterialType::Metal;
    materials[MAT_MIRROR].albedo = { 0.9f, 0.9f, 0.95f };
    materials[MAT_MIRROR].roughness = 0.05f;
    materials[MAT_MIRROR].metallic = 1.0f;

    materials[MAT_DIELECTRIC].type = MaterialType::Dielectric;
    materials[MAT_DIELECTRIC].albedo = { 1.f, 1.f, 1.f };
    materials[MAT_DIELECTRIC].ior = 1.5f;

    materials[MAT_GREEN].type = MaterialType::Lambertian;
    materials[MAT_GREEN].albedo = { 0.f, 1.f, 0.f };
    materials[MAT_GREEN].roughness = 1.0f;

    //VoxLoader::Load("assets/street.vox", *this);

    //VoxelFactory::FlattenInstance(
    //    *this,
    //    0,
    //    float3(128, 128, 128),
    //    float3(-3.14159f / 2.0f, 0, 0),  // -90° around X to convert Z-up to Y-up
    //    float3(1, 1, 1)
    //);

    static float3 spawnMin = { 0.1f, 0.1f, 0.1f };
    static float3 spawnMax = { 0.9f, 0.9f, 0.9f };
    static float  spawnRadius = 0.01f;

    for (int i = 0; i < 1000; ++i)
    {
        spheres.push_back(Sphere{
            float3(
                spawnMin.x + RandomFloat() * (spawnMax.x - spawnMin.x),
                spawnMin.y + RandomFloat() * (spawnMax.y - spawnMin.y),
                spawnMin.z + RandomFloat() * (spawnMax.z - spawnMin.z)
            ),
            spawnRadius,
            MAT_GREEN
            });
    }

    // Spheres are spawned via the ImGui Sphere Spawner panel at runtime.
    // Starting with zero spheres means the BVH is not built and TraceSphereBVH
    // is never called, eliminating 24% of frame time when spheres are absent.
    BuildSphereBVH();
}

// ---------------------------------------------------------------------------
// Voxel setters
// ---------------------------------------------------------------------------

void Scene::Set(uint x, uint y, uint z, uint v)
{
    const uint gx = x / BRICK_SIZE, gy = y / BRICK_SIZE, gz = z / BRICK_SIZE;
    const uint lx = x & (BRICK_SIZE - 1);
    const uint ly = y & (BRICK_SIZE - 1);
    const uint lz = z & (BRICK_SIZE - 1);
    const uint gidx = gx + gy * COARSE_SIZE + gz * COARSE_SIZE2;

    uint cell = coarseGrid[gidx];
    if (cell == 0)
    {
        uint brickIndex = AllocateBrick();
        coarseGrid[gidx] = (brickIndex << 1) | 1;
        cell = coarseGrid[gidx];

        SetOccupied(gidx); // credit: Thomas — occupancy bitmap
    }

    bricks[cell >> 1][lx + ly * BRICK_SIZE + lz * BRICK_SIZE * BRICK_SIZE] = (uint8_t)v;
}

void Scene::SetVoxel(int x, int y, int z, uint value)
{
    if ((uint)x >= WORLDSIZE || (uint)y >= WORLDSIZE || (uint)z >= WORLDSIZE)
        return;

    const uint cx = x >> BRICK_SHIFT;
    const uint cy = y >> BRICK_SHIFT;
    const uint cz = z >> BRICK_SHIFT;
    const uint coarseIdx = cx + cy * COARSE_SIZE + cz * COARSE_SIZE2;

    uint& cell = coarseGrid[coarseIdx];
    if (cell == 0)
    {
        uint newIndex = AllocateBrick();
        cell = (newIndex << 1) | 1;

        SetOccupied(coarseIdx); // credit: Thomas — occupancy bitmap
    }

    uint8_t* brick = bricks[cell >> 1];
    const uint lx = x & (BRICK_SIZE - 1);
    const uint ly = y & (BRICK_SIZE - 1);
    const uint lz = z & (BRICK_SIZE - 1);

    brick[lx + ly * BRICK_SIZE + lz * BRICK_SIZE2] = (uint8_t)value;
}

// ---------------------------------------------------------------------------
// BuildSphereBVH
//
// Rebuilds both the BVH (from AoS) and the SOA (from AoS) in one call.
// The SOA is always consistent with the BVH after this returns.
// ---------------------------------------------------------------------------
void Scene::BuildSphereBVH()
{
    sphereBVHReady = false;

    // Always rebuild the SOA so GetNormal/GetAlbedo are never reading stale data.
    sphereSOA.Rebuild(spheres);

    if (spheres.size() < 2) return;

    g_spheres = spheres.data();

    sphereBVH.Build(SphereAABB, (uint32_t)spheres.size());

    sphereBVHReady = true;
}

// Scalar brute-force fallback (used only when BVH is not ready).
__forceinline static bool IntersectSphere(const Ray& ray, const Sphere& s, float& tHit)
{
    const float3 oc = ray.O - s.center;
    const float  a = dot(ray.D, ray.D);
    const float  b = 2.f * dot(oc, ray.D);
    const float  c = dot(oc, oc) - s.radius * s.radius;
    const float  disc = b * b - 4.f * a * c;

    if (disc < 0.f) return false;

    const float sqrtDisc = sqrtf(disc);
    const float t0 = (-b - sqrtDisc) / (2.f * a);
    const float t1 = (-b + sqrtDisc) / (2.f * a);
    const float t = (t0 > 0.f) ? t0 : t1;

    if (t <= 0.f) return false;
    tHit = t;
    return true;
}

// ---------------------------------------------------------------------------
// DDA setup
// ---------------------------------------------------------------------------

__forceinline bool Scene::Setup3DDDA(Ray& ray, DDAState& state) const
{
    state.t = 0.f;
    const bool startedInGrid = point_in_cube(ray.O);

    if (!startedInGrid)
    {
        state.t = intersect_cube(ray);
        if (state.t > 1e33f) return false;
    }

    static const float cellSize = 1.f / WORLDSIZE;

    state.step = make_int3(
        1 - (int)ray.Dsign.x * 2,
        1 - (int)ray.Dsign.y * 2,
        1 - (int)ray.Dsign.z * 2
    );

    const float3 posInGrid = float3(
        (ray.O.x + (state.t + 0.00005f) * ray.D.x) * WORLDSIZE,
        (ray.O.y + (state.t + 0.00005f) * ray.D.y) * WORLDSIZE,
        (ray.O.z + (state.t + 0.00005f) * ray.D.z) * WORLDSIZE
    );

    const float3 gridPlanes = float3(
        (ceilf(posInGrid.x) - ray.Dsign.x) * cellSize,
        (ceilf(posInGrid.y) - ray.Dsign.y) * cellSize,
        (ceilf(posInGrid.z) - ray.Dsign.z) * cellSize
    );

    state.X = clamp((int)posInGrid.x, 0, WORLDSIZE - 1);
    state.Y = clamp((int)posInGrid.y, 0, WORLDSIZE - 1);
    state.Z = clamp((int)posInGrid.z, 0, WORLDSIZE - 1);

    state.tdelta.x = cellSize * state.step.x / ray.D.x;
    state.tdelta.y = cellSize * state.step.y / ray.D.y;
    state.tdelta.z = cellSize * state.step.z / ray.D.z;

    const float3 offsetO = ray.O + EPSILON * ray.D;
    state.tmax.x = (gridPlanes.x - offsetO.x) / ray.D.x;
    state.tmax.y = (gridPlanes.y - offsetO.y) / ray.D.y;
    state.tmax.z = (gridPlanes.z - offsetO.z) / ray.D.z;

    const uint cell = GetVoxel(state.X, state.Y, state.Z);
    ray.inside = (cell != 0) && startedInGrid;

    return true;
}

// ---------------------------------------------------------------------------
// TraverseDDA — single-loop DDA with coarse brick skip.
//
// Uses Thomas's occupancy bitmap for fast empty-brick rejection.
// ---------------------------------------------------------------------------
template<bool IsOcclusionRay>
__forceinline bool Scene::TraverseDDA(
    Ray& ray,
    float nearestSphereT,
    uint& outMaterial,
    int& outAxis) const
{
    DDAState s;
    if (!Setup3DDDA(ray, s)) return false;
    s.axis = 1; // safe default — overwritten on first step

    const bool startedInside = ray.inside;

    while (true)
    {
        if constexpr (!IsOcclusionRay)
            if (s.t >= nearestSphereT) break;
        if constexpr (IsOcclusionRay)
            if (s.t >= ray.t) return false;

        if (s.X >= (uint)WORLDSIZE || s.Y >= (uint)WORLDSIZE || s.Z >= (uint)WORLDSIZE)
            break;

        // Coarse empty-brick skip using occupancy bitmap (credit: Thomas).
        {
            const uint cx = s.X >> BRICK_SHIFT;
            const uint cy = s.Y >> BRICK_SHIFT;
            const uint cz = s.Z >> BRICK_SHIFT;
            const uint coarseIdx = cx + cy * COARSE_SIZE + cz * COARSE_SIZE2;

            if (!IsOccupied(coarseIdx))
            {
                const int remX = (s.step.x > 0)
                    ? int(BRICK_SIZE - (s.X & (BRICK_SIZE - 1)))
                    : int((s.X & (BRICK_SIZE - 1)) + 1);
                const int remY = (s.step.y > 0)
                    ? int(BRICK_SIZE - (s.Y & (BRICK_SIZE - 1)))
                    : int((s.Y & (BRICK_SIZE - 1)) + 1);
                const int remZ = (s.step.z > 0)
                    ? int(BRICK_SIZE - (s.Z & (BRICK_SIZE - 1)))
                    : int((s.Z & (BRICK_SIZE - 1)) + 1);

                const float txExit = s.tmax.x + float(remX - 1) * s.tdelta.x;
                const float tyExit = s.tmax.y + float(remY - 1) * s.tdelta.y;
                const float tzExit = s.tmax.z + float(remZ - 1) * s.tdelta.z;

                if (txExit < tyExit)
                {
                    if (txExit < tzExit)
                    {
                        s.tmax.x += float(remX) * s.tdelta.x; s.X += s.step.x * remX; s.t = txExit; s.axis = 0;
                    }
                    else
                    {
                        s.tmax.z += float(remZ) * s.tdelta.z; s.Z += s.step.z * remZ; s.t = tzExit; s.axis = 2;
                    }
                }
                else
                {
                    if (tyExit < tzExit)
                    {
                        s.tmax.y += float(remY) * s.tdelta.y; s.Y += s.step.y * remY; s.t = tyExit; s.axis = 1;
                    }
                    else
                    {
                        s.tmax.z += float(remZ) * s.tdelta.z; s.Z += s.step.z * remZ; s.t = tzExit; s.axis = 2;
                    }
                }
                continue;
            }
        }

        const uint cell = GetVoxel(s.X, s.Y, s.Z);

        if constexpr (IsOcclusionRay)
        {
            if (cell && GetMat(cell).type != MaterialType::Dielectric)
                return true;
        }
        else
        {
            if ((startedInside && cell == 0) || (!startedInside && cell != 0))
            {
                if (startedInside)
                {
                    const int hx = (int)s.X - s.step.x;
                    const int hy = (int)s.Y - s.step.y;
                    const int hz = (int)s.Z - s.step.z;
                    if (hx >= 0 && hx < WORLDSIZE &&
                        hy >= 0 && hy < WORLDSIZE &&
                        hz >= 0 && hz < WORLDSIZE)
                    {
                        outMaterial = GetVoxel(hx, hy, hz);
                        outAxis = s.axis;
                    }
                }
                else
                {
                    outMaterial = cell;
                    outAxis = s.axis;
                }
                ray.t = s.t;
                return true;
            }
        }

        // Branchless DDA step: & instead of && avoids short-circuit branches.
        // The compiler maps this to MINPS/CMOV on x86.
        const bool xLTy = s.tmax.x < s.tmax.y;
        const bool xLTz = s.tmax.x < s.tmax.z;
        const bool yLTz = s.tmax.y < s.tmax.z;
        if (xLTy & xLTz)
        {
            s.t = s.tmax.x; s.X += s.step.x; s.tmax.x += s.tdelta.x; s.axis = 0;
        }
        else if (yLTz)
        {
            s.t = s.tmax.y; s.Y += s.step.y; s.tmax.y += s.tdelta.y; s.axis = 1;
        }
        else
        {
            s.t = s.tmax.z; s.Z += s.step.z; s.tmax.z += s.tdelta.z; s.axis = 2;
        }
    }

    return false;
}

// ---------------------------------------------------------------------------
// FindNearest
// ---------------------------------------------------------------------------
void Scene::FindNearest(Ray& ray) const
{
    // ---------------------------------------------------------------------------
    // Fast world-AABB rejection: if the ray misses the [0,1]^3 cube entirely,
    // skip all DDA and sphere work and return a miss immediately.
    // This saves Setup3DDDA + intersect_cube cost for every sky ray.
    // Only runs when the camera is outside the world (the common case for
    // wide-angle shots where most rays hit sky).
    // ---------------------------------------------------------------------------
    if (!point_in_cube(ray.O))
    {
        // Slab test against [0,1]^3
        const float tx1 = (0.f - ray.O.x) * ray.rD.x, tx2 = (1.f - ray.O.x) * ray.rD.x;
        const float ty1 = (0.f - ray.O.y) * ray.rD.y, ty2 = (1.f - ray.O.y) * ray.rD.y;
        const float tz1 = (0.f - ray.O.z) * ray.rD.z, tz2 = (1.f - ray.O.z) * ray.rD.z;
        const float tmin = max(max(min(tx1, tx2), min(ty1, ty2)), min(tz1, tz2));
        const float tmax = min(min(max(tx1, tx2), max(ty1, ty2)), max(tz1, tz2));
        if (tmax < tmin || tmax < 0.f)
        {
            // Ray misses the world cube — guaranteed sky hit.
            ray.t = 1e34f;
            ray.voxel = 0;
            ray.sphereIndex = -1;
            ray.axis = -1;
            ray.materialIndex = -1;
            return;
        }
    }

    float nearestSphereT = 1e34f;
    int   nearestSphereIdx = -1;

    if (sphereSOA.count >= 2 && sphereBVHReady)
    {
        const float3& D = ray.D;
        const float   lenSq = D.x * D.x + D.y * D.y + D.z * D.z;
        if (lenSq > 1e-10f)
        {
            tinybvh::Ray bvhRay(
                { ray.O.x, ray.O.y, ray.O.z },
                { D.x, D.y, D.z }
            );
            TraceSphereBVH(bvhRay);
            if (bvhRay.hit.t < 1e30f)
            {
                nearestSphereT = bvhRay.hit.t;
                nearestSphereIdx = (int)bvhRay.hit.prim;
            }
        }
    }
    else if (sphereSOA.count > 0)
    {
        // Brute-force AVX8 fallback.
        tinybvh::Ray bvhRay(
            { ray.O.x, ray.O.y, ray.O.z },
            { ray.D.x, ray.D.y, ray.D.z }
        );
        float t = nearestSphereT;
        int   idx = IntersectSpheresAVX8(sphereSOA, bvhRay, t);
        if (idx >= 0)
        {
            nearestSphereT = t;
            nearestSphereIdx = idx;
        }
    }

    uint  hitMaterial = 0;
    int   hitAxis = -1;
    float nearestVoxelT = 1e34f;

    ray.t = 1e34f;

    const bool voxelHit = TraverseDDA<false>(ray, nearestSphereT, hitMaterial, hitAxis);
    if (voxelHit) nearestVoxelT = ray.t;

    if (nearestSphereIdx >= 0 && nearestSphereT < nearestVoxelT)
    {
        ray.t = nearestSphereT;
        ray.materialIndex = sphereSOA.material[nearestSphereIdx];
        ray.axis = 3;
        ray.sphereIndex = nearestSphereIdx;
        ray.voxel = 0;
    }
    else if (voxelHit)
    {
        ray.materialIndex = hitMaterial;
        ray.axis = hitAxis;
        ray.sphereIndex = -1;
        ray.voxel = hitMaterial;
    }
    else
    {
        ray.t = 1e34f;
        ray.materialIndex = -1;
        ray.axis = -1;
        ray.sphereIndex = -1;
        ray.voxel = 0;
    }
}

// ---------------------------------------------------------------------------
// IsOccluded
// ---------------------------------------------------------------------------
bool Scene::IsOccluded(Ray& ray) const
{
    uint dummyMat = 0;
    int  dummyAxis = -1;
    return TraverseDDA<true>(ray, 0.f, dummyMat, dummyAxis);
}