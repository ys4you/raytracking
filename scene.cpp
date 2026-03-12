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
    // Expand center by radius in all directions to get the tight AABB
    min = { s.center.x - s.radius, s.center.y - s.radius, s.center.z - s.radius };
    max = { s.center.x + s.radius, s.center.y + s.radius, s.center.z + s.radius };
}

// -----------------------------------------------------------
/// @brief  tinybvh intersection callback: tests a ray against sphere[idx]
///         and updates ray.hit if a closer intersection is found.
///
/// @return True if the ray hits the sphere at a positive t closer than
///         the current ray.hit.t.
// -----------------------------------------------------------
static bool SphereIntersect(tinybvh::Ray& ray, uint32_t idx)
{
    const Sphere& s = Scene::g_spheres[idx];

    float ocx = ray.O.x - s.center.x;
    float ocy = ray.O.y - s.center.y;
    float ocz = ray.O.z - s.center.z;

    float b = ocx * ray.D.x + ocy * ray.D.y + ocz * ray.D.z;
    float oc2 = ocx * ocx + ocy * ocy + ocz * ocz;
    float disc = b * b - (oc2 - s.radius * s.radius);

    if (disc <= 0) return false;

    float sqrtDisc = sqrtf(disc);
    float t = -b - sqrtDisc;
    if (t <= 0) t = -b + sqrtDisc;

    if (t > 0 && t < ray.hit.t)
    {
        ray.hit.t = t;
        ray.hit.prim = idx;
        return true;
    }
    return false;
}

// -----------------------------------------------------------
// Helpers
// -----------------------------------------------------------

/// @brief  Intersects a ray with the unit cube [0,1]³ using the slab method.
///
/// Also records which axis (0=X, 1=Y, 2=Z) produced the entry plane so
/// the DDA can determine the hit normal.
///
/// @return tmin (entry distance) if the ray hits, or 1e34f on a miss.
inline float intersect_cube(Ray& ray)
{
    // X slabs
    const float tx1 = -ray.O.x * ray.rD.x, tx2 = (1 - ray.O.x) * ray.rD.x;
    float ty, tz;
    float tmin = min(tx1, tx2), tmax = max(tx1, tx2);

    // Y slabs
    const float ty1 = -ray.O.y * ray.rD.y, ty2 = (1 - ray.O.y) * ray.rD.y;
    ty = min(ty1, ty2);
    tmin = max(tmin, ty);
    tmax = min(tmax, max(ty1, ty2));

    // Z slabs
    const float tz1 = -ray.O.z * ray.rD.z, tz2 = (1 - ray.O.z) * ray.rD.z;
    tz = min(tz1, tz2);
    tmin = max(tmin, tz);
    tmax = min(tmax, max(tz1, tz2));

    // Record which axis was the last to be entered (determines face normal)
    if (tmin == tz) ray.axis = 2;
    else if (tmin == ty) ray.axis = 1;
    // (else axis = 0, the default)

    return tmax >= tmin ? tmin : 1e34f;
}

/// @brief  Returns true if pos lies strictly inside the unit cube [0,1]³.
inline bool point_in_cube(const float3& pos)
{
    return pos.x >= 0 && pos.y >= 0 && pos.z >= 0 &&
        pos.x <= 1 && pos.y <= 1 && pos.z <= 1;
}

// -----------------------------------------------------------
// Brick grid
// -----------------------------------------------------------

/// @brief  Returns the voxel value at world-grid position (x, y, z).
///
/// The two-level structure works as follows:
///   - The coarse grid stores one cell per BRICK_SIZE³ brick.
///   - If a coarse cell is 0, the entire brick is empty.
///   - If the lowest bit is 0, the brick is a solid colour (value >> 1).
///   - If the lowest bit is 1, the cell is a brick index (index >> 1).
uint Scene::GetVoxel(uint x, uint y, uint z) const
{
    // Coarse-grid coordinates
    uint gx = x / BRICK_SIZE;
    uint gy = y / BRICK_SIZE;
    uint gz = z / BRICK_SIZE;

    uint gidx = gx + gy * COARSE_SIZE + gz * COARSE_SIZE2;
    uint cell = coarseGrid[gidx];

    if (cell == 0) return 0; // entire brick is empty — early out

    if ((cell & 1) == 0)
        return cell >> 1; // solid-colour brick: return the stored colour

    // Fine-grid lookup inside the brick
    uint lx = x & (BRICK_SIZE - 1);
    uint ly = y & (BRICK_SIZE - 1);
    uint lz = z & (BRICK_SIZE - 1);

    return bricks[cell >> 1][lx + ly * BRICK_SIZE + lz * BRICK_SIZE2];
}

/// @brief  Allocates a new zeroed brick and appends it to the brick list.
/// @return Index of the newly allocated brick.
uint Scene::AllocateBrick()
{
    uint8_t* b = (uint8_t*)MALLOC64(BRICK_SIZE3); // 64-byte aligned for SIMD access
    memset(b, 0, BRICK_SIZE3);
    bricks.push_back(b);
    return (uint)bricks.size() - 1;
}

// -----------------------------------------------------------
/// @brief  Scene constructor.  Initialises the brick grid, predefined
///         materials, loads the street.vox asset, and builds the sphere BVH.
// -----------------------------------------------------------
Scene::Scene()
{
    // Allocate and zero the coarse grid
    coarseGrid = (uint*)MALLOC64(COARSE_SIZE3 * sizeof(uint));
    memset(coarseGrid, 0, COARSE_SIZE3 * sizeof(uint));
    bricks.clear();

    // Initialise all material slots to defaults
    materials.fill(Material{});

    // --- Predefined named materials --

    // Polished mirror: near-perfect metal with very low roughness
    materials[MAT_MIRROR].type = MaterialType::Metal;
    materials[MAT_MIRROR].albedo = { 0.9f, 0.9f, 0.95f };
    materials[MAT_MIRROR].roughness = 0.05f;
    materials[MAT_MIRROR].metallic = 1.0f;

    // Glass: dielectric with IOR 1.5 (typical window glass)
    materials[MAT_DIELECTRIC].type = MaterialType::Dielectric;
    materials[MAT_DIELECTRIC].albedo = { 1, 1, 1 };
    materials[MAT_DIELECTRIC].ior = 1.5f;

    // Simple green Lambertian (used as a default for spawned spheres)
    materials[MAT_GREEN].type = MaterialType::Lambertian;
    materials[MAT_GREEN].albedo = { 0, 1, 0 };
    materials[MAT_GREEN].roughness = 1.0f;

    // Load the main voxel scene from disk
    VoxLoader::Load("assets/street.vox", *this);

    // --- Random instance generator (for testing VoxelFactory) ---
    std::random_device rd;
    std::mt19937 gen(rd());

    std::uniform_real_distribution<float> posX(0.0f, 256.0f);
    std::uniform_real_distribution<float> posY(0.0f, 256.0f);
    std::uniform_real_distribution<float> posZ(0.0f, 256.0f);
    std::uniform_real_distribution<float> rot(0.0f, 2 * PI);
    std::uniform_real_distribution<float> scale(0.1f, 1.0f);

    //// Generate 4 random instances (FlattenInstance is commented out for now)
    //for (int i = 0; i < 1; ++i)
    //{
    //    float3 position = { posX(gen), posY(gen), posZ(gen) };
    //    float3 rotation = { rot(gen),  rot(gen),  rot(gen) };
    //    float3 scaleVec = { scale(gen),scale(gen),scale(gen) };

    //    VoxelFactory::FlattenInstance(*this, 0, position, rotation, scaleVec);
    //}

    static float3 spawnMin = { 0.1f, 0.1f, 0.1f }; // minimum world-space corner
    static float3 spawnMax = { 0.9f, 0.9f, 0.9f }; // maximum world-space corner
    static float  spawnRadius = 0.01f;

    for (int i = 0; i < 1000; i++)
    {
        // Place each sphere at a random position within the spawn AABB
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

	//VoxelFactory::FlattenInstance(
	//    *this,
	//    0,
	//    {128, 128, 128},   // shift to be fully positive inside WORLDSIZE
	//    {0, 0, 0},
	//    {1, 1, 1}          // keep scale 1
	//);
    // Build the sphere acceleration structure
    BuildSphereBVH();
}

// -----------------------------------------------------------
/// @brief  Legacy voxel setter (used by old flat-grid code paths).
///
/// Allocates a brick for the coarse cell if one doesn't exist yet,
/// then writes value v into the fine grid.
// -----------------------------------------------------------
void Scene::Set(uint x, uint y, uint z, uint v)
{
    uint gx = x / BRICK_SIZE, gy = y / BRICK_SIZE, gz = z / BRICK_SIZE;
    uint lx = x & (BRICK_SIZE - 1), ly = y & (BRICK_SIZE - 1), lz = z & (BRICK_SIZE - 1);
    uint gidx = gx + gy * COARSE_SIZE + gz * COARSE_SIZE2;
    uint cell = coarseGrid[gidx];

    // Allocate a new brick if the coarse cell is empty
    if (cell == 0)
    {
        uint brickIndex = AllocateBrick();
        coarseGrid[gidx] = (brickIndex << 1) | 1; // tag bit 1 = brick pointer
        cell = coarseGrid[gidx];
    }

    uint brickIndex = cell >> 1;
    bricks[brickIndex][lx + ly * BRICK_SIZE + lz * BRICK_SIZE * BRICK_SIZE] = (uint8_t)v;
}

// -----------------------------------------------------------
/// @brief  Writes a single voxel into the two-level brick grid.
///
/// Silently ignores out-of-bounds coordinates.  Allocates a new
/// brick for the coarse cell if none exists yet.
///
/// @param x, y, z  World-grid coordinates in [0, WORLDSIZE).
/// @param value    Voxel colour/material value (0 = empty).
// -----------------------------------------------------------
void Scene::SetVoxel(int x, int y, int z, uint value)
{
    // Bounds check — cast to uint so negative values fail the >= 0 check too
    if ((uint)x >= WORLDSIZE || (uint)y >= WORLDSIZE || (uint)z >= WORLDSIZE)
        return;

    // Coarse-grid cell coordinates (right-shift by brick size)
    const uint cx = x >> BRICK_SHIFT;
    const uint cy = y >> BRICK_SHIFT;
    const uint cz = z >> BRICK_SHIFT;
    const uint coarseIdx = cx + cy * COARSE_SIZE + cz * COARSE_SIZE2;

    uint& cell = coarseGrid[coarseIdx];

    // Allocate a brick if the coarse cell is still empty
    if (cell == 0)
    {
        uint newIndex = AllocateBrick();
        cell = (newIndex << 1) | 1; // tag bit 1 = this is a brick index
    }

    uint8_t* brick = bricks[cell >> 1]; // dereference the brick index

    // Local coordinates within the brick
    const uint lx = x & (BRICK_SIZE - 1);
    const uint ly = y & (BRICK_SIZE - 1);
    const uint lz = z & (BRICK_SIZE - 1);

    brick[lx + ly * BRICK_SIZE + lz * BRICK_SIZE2] = (uint8_t)value;
}

// -----------------------------------------------------------
// BVH
// -----------------------------------------------------------

/// @brief  (Re)builds the tinybvh custom-geometry BVH over all spheres.
///
/// Must be called after any sphere is added or removed.  Sets
/// sphereBVHReady = false first so FindNearest falls back to the
/// brute-force path during the rebuild.
void Scene::BuildSphereBVH()
{
    sphereBVHReady = false;
    if (spheres.size() < 2) return; // BVH needs at least 2 primitives

    g_spheres = spheres.data(); // expose sphere array to the static callbacks
    sphereBVH.Build(SphereAABB, (uint32_t)spheres.size());
    sphereBVH.customIntersect = SphereIntersect;
    sphereBVHReady = true;
}

// -----------------------------------------------------------
/// @brief  Standalone sphere intersection (legacy, used when the BVH
///         is not ready or sphere count is below the BVH threshold).
///
/// @param ray   The ray to test.
/// @param s     The sphere to test against.
/// @param tHit  Output: distance to the nearest positive intersection.
/// @return      True if the ray hits the sphere at a positive t.
// -----------------------------------------------------------
static bool IntersectSphere(const Ray& ray, const Sphere& s, float& tHit)
{
    float3 oc = ray.O - s.center;
    float  a = dot(ray.D, ray.D);
    float  b = 2.0f * dot(oc, ray.D);
    float  c = dot(oc, oc) - s.radius * s.radius;
    float disc = b * b - 4 * a * c;

    if (disc < 0) return false; // ray misses the sphere

    float sqrtDisc = sqrtf(disc);
    float t0 = (-b - sqrtDisc) / (2 * a); // near root
    float t1 = (-b + sqrtDisc) / (2 * a); // far root
    float t = (t0 > 0) ? t0 : t1;        // prefer the nearer positive root

    if (t <= 0) return false;
    tHit = t;
    return true;
}

// -----------------------------------------------------------
// DDA setup
// -----------------------------------------------------------

/// @brief  Initialises the 3D DDA state for traversing the voxel grid.
///
/// If the ray origin is outside the unit cube, the ray is advanced to
/// the entry point first.  The DDA step, tmax, and tdelta values are
/// pre-computed so the traversal loop only needs comparisons and additions.
bool Scene::Setup3DDDA(Ray& ray, DDAState& state) const
{
    state.t = 0;
    const bool startedInGrid = point_in_cube(ray.O);

    if (!startedInGrid)
    {
        state.t = intersect_cube(ray);
        if (state.t > 1e33f) return false; // ray misses the world entirely
    }

    static const float cellSize = 1.0f / WORLDSIZE;

    // Step direction: +1 or -1 per axis
    state.step = make_int3(
        1 - (int)ray.Dsign.x * 2,
        1 - (int)ray.Dsign.y * 2,
        1 - (int)ray.Dsign.z * 2
    );

    // Entry position in grid space.
    // The 0.00005f epsilon nudges the sample point just past the entry plane
    // so it lands cleanly inside the first voxel rather than on its boundary.
    const float3 posInGrid = float3(
        (ray.O.x + (state.t + 0.00005f) * ray.D.x) * WORLDSIZE,
        (ray.O.y + (state.t + 0.00005f) * ray.D.y) * WORLDSIZE,
        (ray.O.z + (state.t + 0.00005f) * ray.D.z) * WORLDSIZE
    );

    // Next grid plane the ray will cross on each axis
    const float3 gridPlanes = float3(
        (ceilf(posInGrid.x) - ray.Dsign.x) * cellSize,
        (ceilf(posInGrid.y) - ray.Dsign.y) * cellSize,
        (ceilf(posInGrid.z) - ray.Dsign.z) * cellSize
    );

    // Starting voxel, clamped to valid range
    state.X = clamp((int)posInGrid.x, 0, WORLDSIZE - 1);
    state.Y = clamp((int)posInGrid.y, 0, WORLDSIZE - 1);
    state.Z = clamp((int)posInGrid.z, 0, WORLDSIZE - 1);

    // Distance to travel along the ray to cross one voxel on each axis
    state.tdelta.x = cellSize * state.step.x / ray.D.x;
    state.tdelta.y = cellSize * state.step.y / ray.D.y;
    state.tdelta.z = cellSize * state.step.z / ray.D.z;

    // t at which the ray first crosses the next plane on each axis.
    // offsetO shifts the origin by EPSILON along the ray so the DDA never
    // immediately re-hits the surface the ray just left — without mutating ray.O.
    const float3 offsetO = ray.O + EPSILON * ray.D;
    state.tmax.x = (gridPlanes.x - offsetO.x) / ray.D.x;
    state.tmax.y = (gridPlanes.y - offsetO.y) / ray.D.y;
    state.tmax.z = (gridPlanes.z - offsetO.z) / ray.D.z;

    // Flag whether the ray starts inside a filled voxel
    const uint cell = GetVoxel(state.X, state.Y, state.Z);
    ray.inside = cell != 0 && startedInGrid;

    return true;
}
// -----------------------------------------------------------
// FindNearest
// -----------------------------------------------------------

/// @brief  Finds the closest intersection of the ray with the scene
///         (voxels + spheres) and writes the result into the ray.
///
/// Tests spheres first via BVH (or brute-force for small counts),
/// then traverses the voxel grid with 3D DDA.  The closer of the two
/// hits is recorded on the ray.
///
/// On miss: ray.voxel = 0, ray.sphereIndex = -1, ray.t = 1e34f.
void Scene::FindNearest(Ray& ray) const
{

    // -------------------------
    // Sphere intersection
    // -------------------------
    float nearestSphereT = 1e34f;
    int   nearestSphereIdx = -1;

    if (spheres.size() >= 2 && sphereBVHReady)
    {
        const float3& D = ray.D;
        const float lenSq = D.x * D.x + D.y * D.y + D.z * D.z;
        if (lenSq > 1e-10f)
        {
            tinybvh::Ray bvhRay(
                tinybvh::bvhvec3(ray.O.x, ray.O.y, ray.O.z),
                tinybvh::bvhvec3(D.x, D.y, D.z)
            );
            sphereBVH.Intersect(bvhRay);

            if (bvhRay.hit.t < 1e30f)
            {
                nearestSphereT = bvhRay.hit.t;
                nearestSphereIdx = (int)bvhRay.hit.prim;
            }
        }
    }
    else
    {
        // Brute-force fallback for 0 or 1 spheres (BVH requires >= 2)
        for (int i = 0; i < (int)spheres.size(); i++)
        {
            float t;
            if (IntersectSphere(ray, spheres[i], t) && t < nearestSphereT)
            {
                nearestSphereT = t;
                nearestSphereIdx = i;
            }
        }
    }

    // -------------------------
    // Voxel DDA traversal
    // -------------------------
    DDAState s;
    bool  hitVoxel = Setup3DDDA(ray, s);
    float nearestVoxelT = 1e34f;
    uint  hitVoxelMaterial = 0;
    int   voxelHitAxis = -1;

    if (hitVoxel)
    {
        bool startedInside = ray.inside;
        uint cell = 0;

        while (true)
        {
            // Sphere already hit closer than our current traversal distance:
            // no need to march further through voxels.
            if (s.t >= nearestSphereT) break;

            cell = GetVoxel(s.X, s.Y, s.Z);

            // Hit condition depends on whether we started inside or outside:
            //   - outside: stop when we enter a filled voxel
            //   - inside:  stop when we leave a filled voxel (enter empty space)
            if ((startedInside && cell == 0) || (!startedInside && cell != 0))
            {
                nearestVoxelT = s.t;

                if (startedInside)
                {
                    // The actual hit voxel is one step behind the current cell
                    int hitX = s.X - s.step.x;
                    int hitY = s.Y - s.step.y;
                    int hitZ = s.Z - s.step.z;

                    if (hitX >= 0 && hitX < WORLDSIZE &&
                        hitY >= 0 && hitY < WORLDSIZE &&
                        hitZ >= 0 && hitZ < WORLDSIZE)
                    {
                        hitVoxelMaterial = GetVoxel(hitX, hitY, hitZ);
                        voxelHitAxis = s.axis;
                    }
                }
                else
                {
                    hitVoxelMaterial = cell;
                    voxelHitAxis = s.axis;
                }
                break;
            }

            // Advance to the next voxel boundary using the standard DDA step
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

            // Exit loop if the ray has left the world bounds
            if (s.X < 0 || s.X >= WORLDSIZE ||
                s.Y < 0 || s.Y >= WORLDSIZE ||
                s.Z < 0 || s.Z >= WORLDSIZE)
                break;
        }
    }

    // -------------------------
    // Select the nearest hit
    // -------------------------
    if (nearestSphereIdx >= 0 && nearestSphereT < nearestVoxelT)
    {
        // Sphere is closer
        ray.t = nearestSphereT;
        ray.materialIndex = spheres[nearestSphereIdx].material;
        ray.axis = 3; // axis 3 = sphere hit (not a grid face)
        ray.sphereIndex = nearestSphereIdx;
        ray.voxel = 0;
    }
    else if (nearestVoxelT < 1e34f)
    {
        // Voxel is closer (or equal)
        ray.t = nearestVoxelT;
        ray.materialIndex = hitVoxelMaterial;
        ray.axis = voxelHitAxis;
        ray.sphereIndex = -1;
        ray.voxel = hitVoxelMaterial;
    }
    else
    {
        // No hit — sky
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

/// @brief  Fast occlusion test: returns true if any voxel blocks the ray
///         before it reaches its maximum distance (ray.t).
///
/// Does not test spheres.  Used for shadow rays where only a boolean
/// result is needed — no distance or material information is returned.
///
/// @param ray  Shadow ray; ray.t must be set to the light distance.
/// @return     True if the ray is blocked before reaching ray.t.
bool Scene::IsOccluded(Ray& ray) const
{

    DDAState s;
    if (!Setup3DDDA(ray, s)) return false; // ray misses the world

    while (s.t < ray.t) // only traverse up to the light distance
    {
        const uint cell = GetVoxel(s.X, s.Y, s.Z);
        if (cell && GetMat(cell).type != MaterialType::Dielectric)
            return true;
        // Advance to the next voxel boundary
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

        // Exit if the ray has left the world bounds
        if (s.X >= WORLDSIZE || s.Y >= WORLDSIZE || s.Z >= WORLDSIZE) break;
    }

    return false; // ray reached the light without being blocked
}