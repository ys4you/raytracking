#include "template.h"
#include "VoxLoader.h"
#include "Core/Material.h"
#include "Sphere.h"
#include "VoxelFactory.h"

#include <random>

// Static member definition
const Sphere* Scene::g_spheres = nullptr;

// --- tinybvh callbacks (free functions, not class members) ---
static void SphereAABB(uint32_t idx, tinybvh::bvhvec3& min, tinybvh::bvhvec3& max)
{
    const Sphere& s = Scene::g_spheres[idx];
    min = { s.center.x - s.radius, s.center.y - s.radius, s.center.z - s.radius };
    max = { s.center.x + s.radius, s.center.y + s.radius, s.center.z + s.radius };
}

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
    float t = -b - sqrtf(disc);
    if (t <= 0) t = -b + sqrtf(disc);
    if (t > 0 && t < ray.hit.t) { ray.hit.t = t; ray.hit.prim = idx; return true; }
    return false;
}

// -----------------------------------------------------------
// Helpers
// -----------------------------------------------------------
inline float intersect_cube(Ray& ray)
{
    const float tx1 = -ray.O.x * ray.rD.x, tx2 = (1 - ray.O.x) * ray.rD.x;
    float ty, tz, tmin = min(tx1, tx2), tmax = max(tx1, tx2);
    const float ty1 = -ray.O.y * ray.rD.y, ty2 = (1 - ray.O.y) * ray.rD.y;
    ty = min(ty1, ty2), tmin = max(tmin, ty), tmax = min(tmax, max(ty1, ty2));
    const float tz1 = -ray.O.z * ray.rD.z, tz2 = (1 - ray.O.z) * ray.rD.z;
    tz = min(tz1, tz2), tmin = max(tmin, tz), tmax = min(tmax, max(tz1, tz2));
    if (tmin == tz) ray.axis = 2; else if (tmin == ty) ray.axis = 1;
    return tmax >= tmin ? tmin : 1e34f;
}

inline bool point_in_cube(const float3& pos)
{
    return pos.x >= 0 && pos.y >= 0 && pos.z >= 0 &&
        pos.x <= 1 && pos.y <= 1 && pos.z <= 1;
}

// -----------------------------------------------------------
// Brick grid
// -----------------------------------------------------------
uint Scene::GetVoxel(uint x, uint y, uint z) const
{
    uint gx = x / BRICK_SIZE;
    uint gy = y / BRICK_SIZE;
    uint gz = z / BRICK_SIZE;

    uint gidx = gx + gy * COARSE_SIZE + gz * COARSE_SIZE2;
    uint cell = coarseGrid[gidx];
    if (cell == 0) return 0;

    if ((cell & 1) == 0)
        return cell >> 1;

    uint lx = x & (BRICK_SIZE - 1);
    uint ly = y & (BRICK_SIZE - 1);
    uint lz = z & (BRICK_SIZE - 1);

    return bricks[cell >> 1][lx + ly * BRICK_SIZE + lz * BRICK_SIZE2];
}

uint Scene::AllocateBrick()
{
    uint8_t* b = (uint8_t*)MALLOC64(BRICK_SIZE3);
    memset(b, 0, BRICK_SIZE3);
    bricks.push_back(b);
    return (uint)bricks.size() - 1;
}

Scene::Scene()
{
    coarseGrid = (uint*)MALLOC64(COARSE_SIZE3 * sizeof(uint));
    memset(coarseGrid, 0, COARSE_SIZE3 * sizeof(uint));
    bricks.clear();

    materials.fill(Material{});

    // Example predefined materials
    materials[MAT_MIRROR].type = MaterialType::Metal;
    materials[MAT_MIRROR].albedo = { 0.9f, 0.9f, 0.95f };
    materials[MAT_MIRROR].roughness = 0.05f;
    materials[MAT_MIRROR].metallic = 1.0f;

    materials[MAT_DIELECTRIC].type = MaterialType::Dielectric;
    materials[MAT_DIELECTRIC].albedo = { 1,1,1 };
    materials[MAT_DIELECTRIC].ior = 1.5f;

    materials[MAT_GREEN].type = MaterialType::Lambertian;
    materials[MAT_GREEN].albedo = { 0,1,0 };
    materials[MAT_GREEN].roughness = 1.0f;

    VoxLoader::Load("assets/street.vox", *this);

    // Random engine setup
    std::random_device rd;
    std::mt19937 gen(rd());

    // Adjust ranges to fit your scene
    std::uniform_real_distribution<float> posX(0.0f, 256.0f);
    std::uniform_real_distribution<float> posY(0.0f, 256.0f);
    std::uniform_real_distribution<float> posZ(0.0f, 256.0f);

    std::uniform_real_distribution<float> rot(0.0f, 2 * PI);
    std::uniform_real_distribution<float> scale(0.1f, 1.0f); // min 0.1, max 1

    // Generate 1000 random instances
    for (int i = 0; i < 4; ++i)
    {
        float3 position = { posX(gen), posY(gen), posZ(gen) };
        float3 rotation = { rot(gen), rot(gen), rot(gen) };
        float3 scaleVec = { scale(gen), scale(gen), scale(gen) };

        //VoxelFactory::FlattenInstance(*this, 0, position, rotation, scaleVec);
    }

    // --- Build BVH for spheres ---
    BuildSphereBVH();
}
// -----------------------------------------------------------
// Set (legacy, used by old grid code)
// -----------------------------------------------------------
void Scene::Set(uint x, uint y, uint z, uint v)
{
    uint gx = x / BRICK_SIZE, gy = y / BRICK_SIZE, gz = z / BRICK_SIZE;
    uint lx = x & (BRICK_SIZE - 1), ly = y & (BRICK_SIZE - 1), lz = z & (BRICK_SIZE - 1);
    uint gidx = gx + gy * COARSE_SIZE + gz * COARSE_SIZE2;
    uint cell = coarseGrid[gidx];

    if (cell == 0)
    {
        uint brickIndex = AllocateBrick();
        coarseGrid[gidx] = (brickIndex << 1) | 1;
        cell = coarseGrid[gidx];
    }

    uint brickIndex = cell >> 1;
    bricks[brickIndex][lx + ly * BRICK_SIZE + lz * BRICK_SIZE * BRICK_SIZE] = (uint8_t)v;
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
    sphereBVH.customIntersect = SphereIntersect;
    sphereBVHReady = true;
}

// -----------------------------------------------------------
// Sphere intersection (legacy, used internally)
// -----------------------------------------------------------
static bool IntersectSphere(const Ray& ray, const Sphere& s, float& tHit)
{
    float3 oc = ray.O - s.center;
    float a = dot(ray.D, ray.D);
    float b = 2.0f * dot(oc, ray.D);
    float c = dot(oc, oc) - s.radius * s.radius;
    float disc = b * b - 4 * a * c;
    if (disc < 0) return false;
    float sqrtDisc = sqrtf(disc);
    float t0 = (-b - sqrtDisc) / (2 * a);
    float t1 = (-b + sqrtDisc) / (2 * a);
    float t = (t0 > 0) ? t0 : t1;
    if (t <= 0) return false;
    tHit = t;
    return true;
}

// -----------------------------------------------------------
// DDA setup
// -----------------------------------------------------------
bool Scene::Setup3DDDA(Ray& ray, DDAState& state) const
{
    state.t = 0;
    bool startedInGrid = point_in_cube(ray.O);
    if (!startedInGrid)
    {
        state.t = intersect_cube(ray);
        if (state.t > 1e33f) return false;
    }

    static const float cellSize = 1.0f / WORLDSIZE;

    state.step = make_int3(
        1 - ray.Dsign.x * 2,
        1 - ray.Dsign.y * 2,
        1 - ray.Dsign.z * 2
    );

    float3 posInGrid;
    posInGrid.x = (ray.O.x + (state.t + 0.00005f) * ray.D.x) * WORLDSIZE;
    posInGrid.y = (ray.O.y + (state.t + 0.00005f) * ray.D.y) * WORLDSIZE;
    posInGrid.z = (ray.O.z + (state.t + 0.00005f) * ray.D.z) * WORLDSIZE;

    float3 gridPlanes;
    gridPlanes.x = (ceilf(posInGrid.x) - ray.Dsign.x) * cellSize;
    gridPlanes.y = (ceilf(posInGrid.y) - ray.Dsign.y) * cellSize;
    gridPlanes.z = (ceilf(posInGrid.z) - ray.Dsign.z) * cellSize;

    state.X = clamp(int(posInGrid.x), 0, WORLDSIZE - 1);
    state.Y = clamp(int(posInGrid.y), 0, WORLDSIZE - 1);
    state.Z = clamp(int(posInGrid.z), 0, WORLDSIZE - 1);

    state.tdelta.x = cellSize * state.step.x / ray.D.x;
    state.tdelta.y = cellSize * state.step.y / ray.D.y;
    state.tdelta.z = cellSize * state.step.z / ray.D.z;

    state.tmax.x = (gridPlanes.x - ray.O.x) / ray.D.x;
    state.tmax.y = (gridPlanes.y - ray.O.y) / ray.D.y;
    state.tmax.z = (gridPlanes.z - ray.O.z) / ray.D.z;

    uint cell = GetVoxel(state.X, state.Y, state.Z);
    ray.inside = cell != 0 && startedInGrid;

    return true;
}

// -----------------------------------------------------------
// FindNearest
// -----------------------------------------------------------
void Scene::FindNearest(Ray& ray) const
{
    ray.O += EPSILON * ray.D;

    // Sphere BVH query
    float nearestSphereT = 1e34f;
    int nearestSphereIdx = -1;

    if (spheres.size() >= 2 && sphereBVHReady)
    {
        tinybvh::Ray bvhRay(
            tinybvh::bvhvec3(ray.O.x, ray.O.y, ray.O.z),
            tinybvh::bvhvec3(ray.D.x, ray.D.y, ray.D.z)
        );
        sphereBVH.Intersect(bvhRay);
        if (bvhRay.hit.t < 1e30f)
        {
            nearestSphereT = bvhRay.hit.t;
            nearestSphereIdx = (int)bvhRay.hit.prim;
        }
    }
    else
    {
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
    // Voxel DDA query
    DDAState s;
    bool hitVoxel = Setup3DDDA(ray, s);
    float nearestVoxelT = 1e34f;
    uint hitVoxelMaterial = 0;
    int voxelHitAxis = -1;

    if (hitVoxel)
    {
        bool startedInside = ray.inside;
        uint cell = 0;

        while (true)
        {
            cell = GetVoxel(s.X, s.Y, s.Z);

            if ((startedInside && cell == 0) || (!startedInside && cell != 0))
            {
                nearestVoxelT = s.t;
                if (startedInside)
                {
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

            if (s.X < 0 || s.X >= WORLDSIZE ||
                s.Y < 0 || s.Y >= WORLDSIZE ||
                s.Z < 0 || s.Z >= WORLDSIZE)
                break;
        }
    }

    // Pick nearest
    if (nearestSphereIdx >= 0 && nearestSphereT < nearestVoxelT)
    {
        ray.t = nearestSphereT;
        ray.materialIndex = spheres[nearestSphereIdx].material;
        ray.axis = 3;
        ray.sphereIndex = nearestSphereIdx;
        ray.voxel = 0;
    }
    else if (nearestVoxelT < 1e34f)
    {
        ray.t = nearestVoxelT;
        ray.materialIndex = hitVoxelMaterial;
        ray.axis = voxelHitAxis;
        ray.sphereIndex = -1;
        ray.voxel = hitVoxelMaterial;
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
    ray.O += EPSILON * ray.D;
    ray.t -= EPSILON * 2.0f;

    DDAState s;
    if (!Setup3DDDA(ray, s)) return false;

    while (s.t < ray.t)
    {
        const uint cell = GetVoxel(s.X, s.Y, s.Z);
        if (cell) return true;

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

        if (s.X >= WORLDSIZE || s.Y >= WORLDSIZE || s.Z >= WORLDSIZE) break;
    }
    return false;
}