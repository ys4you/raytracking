#include "template.h"
#include "VoxLoader.h"
#include "Core/Material.h"
#include "Sphere.h"
#include "VoxelFactory.h"

#include <random>

// Static member definition — shared pointer used by tinybvh callbacks
// (callbacks are free functions and can't access Scene members directly)
const Sphere* Scene::g_spheres = nullptr;

// -----------------------------------------------------------
/// @brief  tinybvh AABB callback: computes the axis-aligned bounding box
///         of sphere[idx] for BVH construction.
///
/// tinybvh requires plain function pointers, so this cannot be a
/// class member.  g_spheres must be set before calling Build().
// -----------------------------------------------------------
static void SphereAABB(uint32_t idx, tinybvh::bvhvec3& min, tinybvh::bvhvec3& max)
{
    const Sphere& s = Scene::g_spheres[idx];
    min = { s.center.x - s.radius, s.center.y - s.radius, s.center.z - s.radius };
    max = { s.center.x + s.radius, s.center.y + s.radius, s.center.z + s.radius };
}

// -----------------------------------------------------------
/// @brief  Sphere intersection test — private Scene method so it can
///         read this->spheres directly, avoiding the g_spheres static.
///         Force-inlined so the compiler folds it into TraceSphereBVH's
///         leaf loop, eliminating any function-pointer indirection.
///
/// @param ray  tinybvh ray; ray.hit.t is updated on a closer hit.
/// @param idx  Index into this->spheres[].
// -----------------------------------------------------------
__forceinline void Scene::IntersectSphereInlined(tinybvh::Ray& ray, uint32_t idx) const
{
    const Sphere& s = spheres[idx];   // direct member access — no static pointer

    const float ocx = ray.O.x - s.center.x;
    const float ocy = ray.O.y - s.center.y;
    const float ocz = ray.O.z - s.center.z;

    const float b = ocx * ray.D.x + ocy * ray.D.y + ocz * ray.D.z;
    const float oc2 = ocx * ocx + ocy * ocy + ocz * ocz;
    const float disc = b * b - (oc2 - s.radius * s.radius);

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

// -----------------------------------------------------------
/// @brief  Custom BVH traversal — private Scene method.
///
/// Calls IntersectSphereInlined directly at every leaf, eliminating
/// the customIntersect function-pointer call that tinybvh::BVH::Intersect
/// would otherwise use.  Standard ordered stack descent: nearer child first.
///
/// @param ray  tinybvh ray; result written into ray.hit.
// -----------------------------------------------------------
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
            // IntersectSphereInlined is __forceinline — the compiler folds
            // the full intersection math directly into this loop body.
            // primIdx is the correct array name in tinybvh (not triIdx).
            for (uint32_t i = 0; i < node->triCount; ++i)
                IntersectSphereInlined(ray, sphereBVH.primIdx[node->leftFirst + i]);

            if (stackPtr == 0) break;
            node = stack[--stackPtr];
            continue;
        }

        const Node* child1 = &sphereBVH.bvhNode[node->leftFirst];
        const Node* child2 = &sphereBVH.bvhNode[node->leftFirst + 1];

        // BVHNode::Intersect(bvhvec3, bvhvec3) is an AABB-vs-AABB overlap test,
        // not a ray test.  Use tinybvh_intersect_aabb() which takes a Ray and
        // returns the entry distance (BVH_FAR on miss).
        float dist1 = tinybvh::tinybvh_intersect_aabb(ray, child1->aabbMin, child1->aabbMax);
        float dist2 = tinybvh::tinybvh_intersect_aabb(ray, child2->aabbMin, child2->aabbMax);

        // Visit the nearer child first.
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

// -----------------------------------------------------------
// Helpers
// -----------------------------------------------------------

/// @brief  Intersects a ray with the unit cube [0,1]³ using the slab method.
///
/// Records which axis produced the entry plane so the DDA can
/// determine the hit normal.
///
/// @return tmin (entry distance) if the ray hits, or 1e34f on a miss.
__forceinline static float intersect_cube(Ray& ray)
{
    const float tx1 = -ray.O.x * ray.rD.x, tx2 = (1.f - ray.O.x) * ray.rD.x;
    const float ty1 = -ray.O.y * ray.rD.y, ty2 = (1.f - ray.O.y) * ray.rD.y;
    const float tz1 = -ray.O.z * ray.rD.z, tz2 = (1.f - ray.O.z) * ray.rD.z;

    const float ty = min(ty1, ty2);
    const float tz = min(tz1, tz2);
    float tmin = max(max(min(tx1, tx2), ty), tz);
    float tmax = min(min(max(tx1, tx2), max(ty1, ty2)), max(tz1, tz2));

    // Record which axis the ray entered last — used as the face normal.
    if (tmin == tz) ray.axis = 2;
    else if (tmin == ty) ray.axis = 1;
    // else axis = 0 (default X)

    return tmax >= tmin ? tmin : 1e34f;
}

/// @brief  Returns true if pos lies inside the unit cube [0,1]³.
__forceinline static bool point_in_cube(const float3& pos)
{
    return pos.x >= 0.f && pos.y >= 0.f && pos.z >= 0.f
        && pos.x <= 1.f && pos.y <= 1.f && pos.z <= 1.f;
}

// -----------------------------------------------------------
// Brick grid
// -----------------------------------------------------------

/// @brief  Returns the voxel value at world-grid position (x, y, z).
///
/// Two-level structure:
///   - coarseGrid cell == 0          → entire brick is empty.
///   - (cell & 1) == 0               → solid-colour brick; value = cell >> 1.
///   - (cell & 1) == 1               → brick pointer;      index = cell >> 1.
uint Scene::GetVoxel(uint x, uint y, uint z) const
{
    const uint gidx = (x / BRICK_SIZE)
        + (y / BRICK_SIZE) * COARSE_SIZE
        + (z / BRICK_SIZE) * COARSE_SIZE2;
    const uint cell = coarseGrid[gidx];

    if (cell == 0)        return 0;           // empty brick — early out
    if ((cell & 1) == 0)  return cell >> 1;   // solid-colour brick

    const uint lx = x & (BRICK_SIZE - 1);
    const uint ly = y & (BRICK_SIZE - 1);
    const uint lz = z & (BRICK_SIZE - 1);

    return bricks[cell >> 1][lx + ly * BRICK_SIZE + lz * BRICK_SIZE2];
}

/// @brief  Allocates a new zeroed brick and appends it to the brick list.
uint Scene::AllocateBrick()
{
    uint8_t* b = (uint8_t*)MALLOC64(BRICK_SIZE3);
    memset(b, 0, BRICK_SIZE3);
    bricks.push_back(b);
    return (uint)bricks.size() - 1;
}

// -----------------------------------------------------------
// Scene constructor
// -----------------------------------------------------------
Scene::Scene()
{
    coarseGrid = (uint*)MALLOC64(COARSE_SIZE3 * sizeof(uint));
    memset(coarseGrid, 0, COARSE_SIZE3 * sizeof(uint));
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

    VoxLoader::Load("assets/street.vox", *this);

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

    BuildSphereBVH();
}

// -----------------------------------------------------------
// Voxel setters
// -----------------------------------------------------------

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
    }

    uint8_t* brick = bricks[cell >> 1];
    const uint lx = x & (BRICK_SIZE - 1);
    const uint ly = y & (BRICK_SIZE - 1);
    const uint lz = z & (BRICK_SIZE - 1);

    brick[lx + ly * BRICK_SIZE + lz * BRICK_SIZE2] = (uint8_t)value;
}

// -----------------------------------------------------------
// BVH
// -----------------------------------------------------------

void Scene::BuildSphereBVH()
{
    sphereBVHReady = false;
    if (spheres.size() < 2) return;

    g_spheres = spheres.data();
    sphereBVH.Build(SphereAABB, (uint32_t)spheres.size());
    // Note: we no longer set sphereBVH.customIntersect — TraceSphereBVH
    // replaces the library's Intersect() call entirely.
    sphereBVHReady = true;
}

// Legacy brute-force sphere test (fallback when BVH is not ready).
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

// -----------------------------------------------------------
// DDA setup
// -----------------------------------------------------------

/// @brief  Initialises the 3D DDA state for traversing the voxel grid.
///         Force-inlined because it is called from the hot TraverseDDA
///         template and we want the compiler to constant-fold its outputs
///         into the DDA loop registers.
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

// -----------------------------------------------------------
// Templated DDA traversal
// -----------------------------------------------------------
//
// TraverseDDA<false> = FindNearest:  record the closest voxel hit.
// TraverseDDA<true>  = IsOccluded:  return true on any opaque hit
//                                   before ray.t (shadow ray distance).
//
// The template parameter is resolved at compile time by if constexpr,
// so each instantiation compiles to a branch-free, fully-optimised loop
// with no dead code — no runtime overhead for the unused path.
//
// The function is force-inlined so that when FindNearest / IsOccluded
// call it, the compiler can optimise across the call boundary and
// allocate all DDA state in registers.

template<bool IsOcclusionRay>
__forceinline bool Scene::TraverseDDA(
    Ray& ray,
    float nearestSphereT,
    uint& outMaterial,
    int& outAxis) const
{
    DDAState s;
    if (!Setup3DDDA(ray, s)) return false;

    const bool startedInside = ray.inside;

    while (true)
    {
        // --- Early exit: sphere already closer than current voxel t ---
        // Only needed for FindNearest; IsOccluded has its own distance limit.
        if constexpr (!IsOcclusionRay)
        {
            if (s.t >= nearestSphereT) break;
        }

        // --- IsOccluded distance limit: stop at the light ---
        if constexpr (IsOcclusionRay)
        {
            if (s.t >= ray.t) return false;
        }

        const uint cell = GetVoxel(s.X, s.Y, s.Z);

        // --- Hit test ---
        if constexpr (IsOcclusionRay)
        {
            // Glass (Dielectric) does not cast shadows.
            if (cell && GetMat(cell).type != MaterialType::Dielectric)
                return true;
        }
        else
        {
            // For FindNearest the hit condition flips depending on whether
            // the ray started inside a solid voxel:
            //   - outside → hit when we enter a filled voxel
            //   - inside  → hit when we leave into empty space
            if ((startedInside && cell == 0) || (!startedInside && cell != 0))
            {
                if (startedInside)
                {
                    // Step back one voxel to find the surface we just left.
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

        // --- DDA step: advance to the next voxel boundary ---
        if (s.tmax.x < s.tmax.y)
        {
            if (s.tmax.x < s.tmax.z) { s.t = s.tmax.x; s.X += s.step.x; s.tmax.x += s.tdelta.x; s.axis = 0; }
            else { s.t = s.tmax.z; s.Z += s.step.z; s.tmax.z += s.tdelta.z; s.axis = 2; }
        }
        else
        {
            if (s.tmax.y < s.tmax.z) { s.t = s.tmax.y; s.Y += s.step.y; s.tmax.y += s.tdelta.y; s.axis = 1; }
            else { s.t = s.tmax.z; s.Z += s.step.z; s.tmax.z += s.tdelta.z; s.axis = 2; }
        }

        // --- Bounds check ---
        if (s.X >= (uint)WORLDSIZE || s.Y >= (uint)WORLDSIZE || s.Z >= (uint)WORLDSIZE)
            break;
    }

    return false;
}

// -----------------------------------------------------------
// FindNearest
// -----------------------------------------------------------

void Scene::FindNearest(Ray& ray) const
{
    // ---- Sphere intersection ----
    float nearestSphereT = 1e34f;
    int   nearestSphereIdx = -1;

    if (spheres.size() >= 2 && sphereBVHReady)
    {
        const float3& D = ray.D;
        const float   lenSq = D.x * D.x + D.y * D.y + D.z * D.z;
        if (lenSq > 1e-10f)
        {
            tinybvh::Ray bvhRay(
                { ray.O.x, ray.O.y, ray.O.z },
                { D.x,     D.y,     D.z }
            );

            // Private method — IntersectSphereInlined is folded directly
            // into the leaf loop, eliminating the customIntersect pointer call.
            TraceSphereBVH(bvhRay);

            if (bvhRay.hit.t < 1e30f)
            {
                nearestSphereT = bvhRay.hit.t;
                nearestSphereIdx = (int)bvhRay.hit.prim;
            }
        }
    }
    else
    {
        for (int i = 0; i < (int)spheres.size(); ++i)
        {
            float t;
            if (IntersectSphere(ray, spheres[i], t) && t < nearestSphereT)
            {
                nearestSphereT = t;
                nearestSphereIdx = i;
            }
        }
    }

    // ---- Voxel DDA traversal ----
    uint  hitMaterial = 0;
    int   hitAxis = -1;
    float nearestVoxelT = 1e34f;

    // Temporarily store ray.t so TraverseDDA<false> can write to it.
    ray.t = 1e34f;

    const bool voxelHit = TraverseDDA<false>(ray, nearestSphereT, hitMaterial, hitAxis);
    if (voxelHit) nearestVoxelT = ray.t;

    // ---- Select the nearest hit ----
    if (nearestSphereIdx >= 0 && nearestSphereT < nearestVoxelT)
    {
        ray.t = nearestSphereT;
        ray.materialIndex = spheres[nearestSphereIdx].material;
        ray.axis = 3; // axis 3 = sphere (not a voxel face)
        ray.sphereIndex = nearestSphereIdx;
        ray.voxel = 0;
    }
    else if (voxelHit)
    {
        // ray.t already written by TraverseDDA<false>
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

// -----------------------------------------------------------
// IsOccluded
// -----------------------------------------------------------

bool Scene::IsOccluded(Ray& ray) const
{
    // TraverseDDA<true> reads ray.t as the maximum shadow ray distance
    // and returns true immediately on the first opaque voxel hit.
    // The dummy outMaterial / outAxis variables are optimised away by the
    // compiler because the IsOcclusionRay branch never writes them.
    uint dummyMat = 0;
    int  dummyAxis = -1;
    return TraverseDDA<true>(ray, 0.f, dummyMat, dummyAxis);
}