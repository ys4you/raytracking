#include "template.h"
#include "VoxLoader.h"
#include "Core/Material.h"
#include "Sphere.h"
#include "VoxelFactory.h"

#include <random>
#include <immintrin.h>


const Sphere* Scene::g_spheres = nullptr;

const VoxelInstance* Scene::g_instances = nullptr;

static void SphereAABB(uint32_t idx, tinybvh::bvhvec3& mn, tinybvh::bvhvec3& mx)
{
    const Sphere& s = Scene::g_spheres[idx];
    mn = { s.center.x - s.radius, s.center.y - s.radius, s.center.z - s.radius };
    mx = { s.center.x + s.radius, s.center.y + s.radius, s.center.z + s.radius };
}


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
        if (dist1 > dist2) { tinybvh::tinybvh_swap(dist1, dist2); tinybvh::tinybvh_swap(child1, child2); }
        if (dist1 == BVH_FAR)
        {
            if (stackPtr == 0) break;
            node = stack[--stackPtr];
        }
        else
        {
            node = child1;
            if (dist2 != BVH_FAR) stack[stackPtr++] = child2;
        }
    }
}

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
    if (t > 0.f && t < ray.hit.t) { ray.hit.t = t; ray.hit.prim = idx; }
}

void Scene::TraceSphereGrid(Ray& ray, float& nearestT, int& nearestIdx) const
{
    const float3& bMin = sphereGrid.boundsMin;
    const float3& bMax = sphereGrid.boundsMax;
    const float tx1 = (bMin.x - ray.O.x) * ray.rD.x, tx2 = (bMax.x - ray.O.x) * ray.rD.x;
    const float ty1 = (bMin.y - ray.O.y) * ray.rD.y, ty2 = (bMax.y - ray.O.y) * ray.rD.y;
    const float tz1 = (bMin.z - ray.O.z) * ray.rD.z, tz2 = (bMax.z - ray.O.z) * ray.rD.z;
    float tmin = max(max(min(tx1, tx2), min(ty1, ty2)), min(tz1, tz2));
    float tmax = min(min(max(tx1, tx2), max(ty1, ty2)), max(tz1, tz2));
    if (tmax < 0.f || tmin > tmax || tmin > nearestT) return;

    constexpr int   RES = SphereGrid::SGRID_RES;
    constexpr float CELL = SphereGrid::CELL_SIZE;
    float entryT = max(tmin, 0.f);
    float3 entryPos = ray.O + (entryT + 0.0001f) * ray.D;
    int X = clamp((int)(entryPos.x / CELL), 0, RES - 1);
    int Y = clamp((int)(entryPos.y / CELL), 0, RES - 1);
    int Z = clamp((int)(entryPos.z / CELL), 0, RES - 1);
    const int stepX = (ray.D.x >= 0.f) ? 1 : -1;
    const int stepY = (ray.D.y >= 0.f) ? 1 : -1;
    const int stepZ = (ray.D.z >= 0.f) ? 1 : -1;
    const float nextX = ((ray.D.x >= 0.f) ? (X + 1) : X) * CELL;
    const float nextY = ((ray.D.y >= 0.f) ? (Y + 1) : Y) * CELL;
    const float nextZ = ((ray.D.z >= 0.f) ? (Z + 1) : Z) * CELL;
    float tmaxX = (nextX - ray.O.x) * ray.rD.x;
    float tmaxY = (nextY - ray.O.y) * ray.rD.y;
    float tmaxZ = (nextZ - ray.O.z) * ray.rD.z;
    const float tdeltaX = CELL * fabsf(ray.rD.x);
    const float tdeltaY = CELL * fabsf(ray.rD.y);
    const float tdeltaZ = CELL * fabsf(ray.rD.z);

    while (true)
    {
        const float cellEntry = max(max(tmaxX - tdeltaX, tmaxY - tdeltaY), tmaxZ - tdeltaZ);
        if (cellEntry > nearestT) break;
        const int cellIdx = X + Y * RES + Z * SphereGrid::SGRID_RES2;
        const uint32_t cnt = sphereGrid.cellCount[cellIdx];
        if (cnt > 0)
        {
            const uint32_t start = sphereGrid.cellStart[cellIdx];
            const uint32_t* ids = sphereGrid.indices.data() + start;
            for (uint32_t j = 0; j < cnt; j++)
            {
                const uint32_t i = ids[j];
                const float ocx = ray.O.x - sphereSOA.cx[i];
                const float ocy = ray.O.y - sphereSOA.cy[i];
                const float ocz = ray.O.z - sphereSOA.cz[i];
                const float b = ocx * ray.D.x + ocy * ray.D.y + ocz * ray.D.z;
                const float oc2 = ocx * ocx + ocy * ocy + ocz * ocz;
                const float disc = b * b - (oc2 - sphereSOA.r2[i]);
                if (disc <= 0.f) continue;
                const float sqrtDisc = sqrtf(disc);
                float t = -b - sqrtDisc;
                if (t <= 0.f) t = -b + sqrtDisc;
                if (t > 0.f && t < nearestT)
                {
                    nearestT = t; nearestIdx = (int)i;
                }
            }
        }
        if (tmaxX < tmaxY) {
            if (tmaxX < tmaxZ) { X += stepX; if ((uint)X >= (uint)RES) break; tmaxX += tdeltaX; }
            else { Z += stepZ; if ((uint)Z >= (uint)RES) break; tmaxZ += tdeltaZ; }
        }
        else {
            if (tmaxY < tmaxZ) { Y += stepY; if ((uint)Y >= (uint)RES) break; tmaxY += tdeltaY; }
            else { Z += stepZ; if ((uint)Z >= (uint)RES) break; tmaxZ += tdeltaZ; }
        }
    }
}



__forceinline static float intersect_cube(Ray& ray)
{
    const float tx1 = -ray.O.x * ray.rD.x, tx2 = (1.f - ray.O.x) * ray.rD.x;
    const float ty1 = -ray.O.y * ray.rD.y, ty2 = (1.f - ray.O.y) * ray.rD.y;
    const float tz1 = -ray.O.z * ray.rD.z, tz2 = (1.f - ray.O.z) * ray.rD.z;
    const float ty = min(ty1, ty2), tz = min(tz1, tz2);
    float tmin = max(max(min(tx1, tx2), ty), tz);
    float tmax = min(min(max(tx1, tx2), max(ty1, ty2)), max(tz1, tz2));
    // Remember which slab produced entry time so shading can reconstruct a face normal.
    if (tmin == tz) ray.axis = 2;
    else if (tmin == ty) ray.axis = 1;
    return tmax >= tmin ? tmin : 1e34f;
}

__forceinline static bool point_in_cube(const float3& pos)
{
    return pos.x >= 0.f && pos.y >= 0.f && pos.z >= 0.f
        && pos.x <= 1.f && pos.y <= 1.f && pos.z <= 1.f;
}



uint Scene::GetVoxel(uint x, uint y, uint z) const
{
    const uint gidx = (x / BRICK_SIZE) + (y / BRICK_SIZE) * COARSE_SIZE + (z / BRICK_SIZE) * COARSE_SIZE2;
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
    uint8_t* b = static_cast<uint8_t*>(MALLOC64(BRICK_SIZE3));
    memset(b, 0, BRICK_SIZE3);
    bricks.push_back(b);
    return static_cast<uint>(bricks.size() - 1);
}



Scene::Scene()
{
    coarseGrid = static_cast<uint*>(MALLOC64(COARSE_SIZE3 * sizeof(uint)));
    memset(coarseGrid, 0, COARSE_SIZE3 * sizeof(uint));
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

    materials[MAT_LAMBERTIAN_WHITE].type = MaterialType::Emissive;
    materials[MAT_LAMBERTIAN_WHITE].albedo = { 1.0f, 1.0f, 1.0f };
    materials[MAT_LAMBERTIAN_WHITE].emission = { 1.0f, 1.0f, 1.0f };
    materials[MAT_LAMBERTIAN_WHITE].emissionStr = 3.0f;

    for (uint i = MAT_RANDOM_START; i < MAT_COUNT; i++)
    {
        materials[i].type = MaterialType::Lambertian;
        materials[i].albedo = float3(RandomFloat(), RandomFloat(), RandomFloat());
        materials[i].roughness = 1.0f;
    }

    BuildSphereBVH();

}



void Scene::Set(uint x, uint y, uint z, uint v)
{
    const uint gx = x / BRICK_SIZE, gy = y / BRICK_SIZE, gz = z / BRICK_SIZE;
    const uint lx = x & (BRICK_SIZE - 1), ly = y & (BRICK_SIZE - 1), lz = z & (BRICK_SIZE - 1);
    const uint gidx = gx + gy * COARSE_SIZE + gz * COARSE_SIZE2;
    uint cell = coarseGrid[gidx];
    if (cell == 0)
    {
        uint brickIndex = AllocateBrick();
        coarseGrid[gidx] = (brickIndex << 1) | 1;
        cell = coarseGrid[gidx];
        SetOccupied(gidx);
    }
    bricks[cell >> 1][lx + ly * BRICK_SIZE + lz * BRICK_SIZE * BRICK_SIZE] = (uint8_t)v;
}

void Scene::SetVoxel(int x, int y, int z, uint value)
{
    if ((uint)x >= WORLDSIZE || (uint)y >= WORLDSIZE || (uint)z >= WORLDSIZE) return;
    const uint cx = x >> BRICK_SHIFT, cy = y >> BRICK_SHIFT, cz = z >> BRICK_SHIFT;
    const uint coarseIdx = cx + cy * COARSE_SIZE + cz * COARSE_SIZE2;
    uint& cell = coarseGrid[coarseIdx];
    if (cell == 0) { uint ni = AllocateBrick(); cell = (ni << 1) | 1; SetOccupied(coarseIdx); }
    bricks[cell >> 1][(x & (BRICK_SIZE - 1)) + (y & (BRICK_SIZE - 1)) * BRICK_SIZE + (z & (BRICK_SIZE - 1)) * BRICK_SIZE2] = (uint8_t)value;
}

void Scene::BuildSphereBVH()
{
    sphereBVHReady = false;
    sphereSOA.Rebuild(spheres);
    sphereGrid.Build(spheres);
    if (spheres.size() >= 2)
    {
        g_spheres = spheres.data();
        sphereBVH.Build(SphereAABB, (uint32_t)spheres.size());
        sphereBVHReady = true;
    }
}

static void InstanceAABB(uint32_t idx, tinybvh::bvhvec3& mn, tinybvh::bvhvec3& mx)
{
    const VoxelInstance& inst = Scene::g_instances[idx];
    mn = { inst.worldAABBmin.x, inst.worldAABBmin.y, inst.worldAABBmin.z };
    mx = { inst.worldAABBmax.x, inst.worldAABBmax.y, inst.worldAABBmax.z };
}

void Scene::BuildInstanceBVH()
{
    instanceBVHReady = false;
    if (voxelInstances.size() < 2) return;
    g_instances = voxelInstances.data();
    instanceBVH.Build(InstanceAABB, (uint32_t)voxelInstances.size());
    instanceBVHReady = true;
}



void Scene::RebuildDirtyInstances()
{
    bool anyRebuilt = false;
    for (auto& inst : voxelInstances)
    {
        if (!inst.matricesDirty) continue;
        if (inst.modelIndex < 0 || inst.modelIndex >= (int)voxelObjects.size()) continue;
        const VoxelObject& obj = voxelObjects[inst.modelIndex];
        inst.BuildMatrices(obj.sizeX, obj.sizeY, obj.sizeZ);
        anyRebuilt = true;
    }
    if (anyRebuilt || !instanceBVHReady)
        BuildInstanceBVH();
}



__forceinline bool Scene::Setup3DDDA(Ray& ray, DDAState& state) const
{
    state.t = 0.f;
    const bool startedInGrid = point_in_cube(ray.O);
    if (!startedInGrid) { state.t = intersect_cube(ray); if (state.t > 1e33f) return false; }
    static const float cellSize = 1.f / WORLDSIZE;
    // Convert packed sign bits (0/1) to DDA step signs (+1/-1).
    state.step = make_int3(1 - (int)ray.Dsign.x * 2, 1 - (int)ray.Dsign.y * 2, 1 - (int)ray.Dsign.z * 2);
    const float3 posInGrid = float3(
        (ray.O.x + (state.t + 0.00005f) * ray.D.x) * WORLDSIZE,
        (ray.O.y + (state.t + 0.00005f) * ray.D.y) * WORLDSIZE,
        (ray.O.z + (state.t + 0.00005f) * ray.D.z) * WORLDSIZE);
    const float3 gridPlanes = float3(
        (ceilf(posInGrid.x) - ray.Dsign.x) * cellSize,
        (ceilf(posInGrid.y) - ray.Dsign.y) * cellSize,
        (ceilf(posInGrid.z) - ray.Dsign.z) * cellSize);
    state.X = clamp((int)posInGrid.x, 0, WORLDSIZE - 1);
    state.Y = clamp((int)posInGrid.y, 0, WORLDSIZE - 1);
    state.Z = clamp((int)posInGrid.z, 0, WORLDSIZE - 1);
    state.tdelta = float3(cellSize * state.step.x / ray.D.x,
        cellSize * state.step.y / ray.D.y,
        cellSize * state.step.z / ray.D.z);
    const float3 offsetO = ray.O + EPSILON * ray.D;
    state.tmax = float3((gridPlanes.x - offsetO.x) / ray.D.x,
        (gridPlanes.y - offsetO.y) / ray.D.y,
        (gridPlanes.z - offsetO.z) / ray.D.z);
    const uint cell = GetVoxel(state.X, state.Y, state.Z);
    ray.inside = (cell != 0) && startedInGrid;
    return true;
}

template<bool IsOcclusionRay>
__forceinline bool Scene::TraverseDDA(
    Ray& ray, float nearestSphereT, uint& outMaterial, int& outAxis) const
{
    DDAState s;
    if (!Setup3DDDA(ray, s)) return false;
    s.axis = 1;
    const bool startedInside = ray.inside;

    while (true)
    {
        if constexpr (!IsOcclusionRay) { if (s.t >= nearestSphereT) break; }
        if constexpr (IsOcclusionRay) { if (s.t >= ray.t) return false; }
        if (s.X >= (uint)WORLDSIZE || s.Y >= (uint)WORLDSIZE || s.Z >= (uint)WORLDSIZE) break;

        {
            const uint cx = s.X >> BRICK_SHIFT, cy = s.Y >> BRICK_SHIFT, cz = s.Z >> BRICK_SHIFT;
            const uint coarseIdx = cx + cy * COARSE_SIZE + cz * COARSE_SIZE2;
            if (!IsOccupied(coarseIdx))
            {
                // Skip the remainder of the current coarse brick in one step.
                const int remX = (s.step.x > 0) ? (int)(BRICK_SIZE - (s.X & (BRICK_SIZE - 1))) : (int)((s.X & (BRICK_SIZE - 1)) + 1);
                const int remY = (s.step.y > 0) ? (int)(BRICK_SIZE - (s.Y & (BRICK_SIZE - 1))) : (int)((s.Y & (BRICK_SIZE - 1)) + 1);
                const int remZ = (s.step.z > 0) ? (int)(BRICK_SIZE - (s.Z & (BRICK_SIZE - 1))) : (int)((s.Z & (BRICK_SIZE - 1)) + 1);
                const float txE = s.tmax.x + (float)(remX - 1) * s.tdelta.x;
                const float tyE = s.tmax.y + (float)(remY - 1) * s.tdelta.y;
                const float tzE = s.tmax.z + (float)(remZ - 1) * s.tdelta.z;
                if (txE < tyE) {
                    if (txE < tzE) { s.tmax.x += (float)remX * s.tdelta.x; s.X += s.step.x * remX; s.t = txE; s.axis = 0; }
                    else { s.tmax.z += (float)remZ * s.tdelta.z; s.Z += s.step.z * remZ; s.t = tzE; s.axis = 2; }
                }
                else {
                    if (tyE < tzE) { s.tmax.y += (float)remY * s.tdelta.y; s.Y += s.step.y * remY; s.t = tyE; s.axis = 1; }
                    else { s.tmax.z += (float)remZ * s.tdelta.z; s.Z += s.step.z * remZ; s.t = tzE; s.axis = 2; }
                }
                continue;
            }
        }

        const uint cell = GetVoxel(s.X, s.Y, s.Z);
        if constexpr (IsOcclusionRay)
        {
            if (cell && GetMat(cell).type != MaterialType::Dielectric) return true;
        }
        else
        {
            if ((startedInside && cell == 0) || (!startedInside && cell != 0))
            {
                if (startedInside) {
                    // Inside-out case: use the previous voxel cell as the material source.
                    const int hx = (int)s.X - s.step.x, hy = (int)s.Y - s.step.y, hz = (int)s.Z - s.step.z;
                    if (hx >= 0 && hx < WORLDSIZE && hy >= 0 && hy < WORLDSIZE && hz >= 0 && hz < WORLDSIZE)
                    {
                        outMaterial = GetVoxel(hx, hy, hz); outAxis = s.axis;
                    }
                }
                else { outMaterial = cell; outAxis = s.axis; }
                ray.t = s.t;
                return true;
            }
        }

        // Use branch-minimal axis selection in the hot traversal loop.
        const bool xLTy = s.tmax.x < s.tmax.y, xLTz = s.tmax.x < s.tmax.z, yLTz = s.tmax.y < s.tmax.z;
        if (xLTy & xLTz) { s.t = s.tmax.x; s.X += s.step.x; s.tmax.x += s.tdelta.x; s.axis = 0; }
        else if (yLTz) { s.t = s.tmax.y; s.Y += s.step.y; s.tmax.y += s.tdelta.y; s.axis = 1; }
        else { s.t = s.tmax.z; s.Z += s.step.z; s.tmax.z += s.tdelta.z; s.axis = 2; }
    }
    return false;
}



float Scene::TraceObjectDDA(
    const float3& localO, const float3& localD,
    const VoxelObject& obj, float tMax,
    int& outFace, uint8_t& outVoxel) const
{
    const float3 bmax = float3((float)obj.sizeX, (float)obj.sizeY, (float)obj.sizeZ);

    const float3 rD = float3(
        (fabsf(localD.x) > 1e-20f) ? 1.0f / localD.x : 1e30f,
        (fabsf(localD.y) > 1e-20f) ? 1.0f / localD.y : 1e30f,
        (fabsf(localD.z) > 1e-20f) ? 1.0f / localD.z : 1e30f);

    const float tx1 = -localO.x * rD.x, tx2 = (bmax.x - localO.x) * rD.x;
    const float ty1 = -localO.y * rD.y, ty2 = (bmax.y - localO.y) * rD.y;
    const float tz1 = -localO.z * rD.z, tz2 = (bmax.z - localO.z) * rD.z;
    float tEntry = max(max(min(tx1, tx2), min(ty1, ty2)), min(tz1, tz2));
    float tExit = min(min(max(tx1, tx2), max(ty1, ty2)), max(tz1, tz2));

    if (tExit < tEntry || tExit < 0.f || tEntry >= tMax)
        return tMax;

    tEntry = max(tEntry, 0.f);

    const float3 entry = localO + localD * (tEntry + 1e-4f);
    // Nudge by epsilon to avoid re-hitting the entry boundary voxel.
    int X = clamp((int)floorf(entry.x), 0, (int)obj.sizeX - 1);
    int Y = clamp((int)floorf(entry.y), 0, (int)obj.sizeY - 1);
    int Z = clamp((int)floorf(entry.z), 0, (int)obj.sizeZ - 1);

    const int stepX = (localD.x >= 0.f) ? 1 : -1;
    const int stepY = (localD.y >= 0.f) ? 1 : -1;
    const int stepZ = (localD.z >= 0.f) ? 1 : -1;

    float tmaxX = (((stepX > 0) ? (X + 1.f) : (float)X) - localO.x) * rD.x;
    float tmaxY = (((stepY > 0) ? (Y + 1.f) : (float)Y) - localO.y) * rD.y;
    float tmaxZ = (((stepZ > 0) ? (Z + 1.f) : (float)Z) - localO.z) * rD.z;

    const float tdeltaX = fabsf(rD.x);
    const float tdeltaY = fabsf(rD.y);
    const float tdeltaZ = fabsf(rD.z);

    int lastAxis = -1;

    while (true)
    {
        const uint8_t v = obj.Get((uint)X, (uint)Y, (uint)Z);
        if (v != 0)
        {
            float hitT = (lastAxis < 0) ? max(tEntry, 0.f) :
                (lastAxis == 0) ? tmaxX - tdeltaX :
                (lastAxis == 1) ? tmaxY - tdeltaY :
                tmaxZ - tdeltaZ;
            if (hitT < tMax)
            {
                if (lastAxis < 0)
                {
                    // If we start inside the first occupied voxel, pick the nearest face.
                    float3 hitLocal = localO + localD * max(tEntry, 0.f);
                    float dists[6] = {
                        (float)obj.sizeX - hitLocal.x,
                        hitLocal.x,
                        (float)obj.sizeY - hitLocal.y,
                        hitLocal.y,
                        (float)obj.sizeZ - hitLocal.z,
                        hitLocal.z
                    };
                    outFace = 0;
                    float minD = dists[0];
                    for (int f = 1; f < 6; f++)
                        if (dists[f] < minD) { minD = dists[f]; outFace = f; }
                }
                else
                {
                    if (lastAxis == 0)      outFace = (stepX > 0) ? 1 : 0;
                    else if (lastAxis == 1)  outFace = (stepY > 0) ? 3 : 2;
                    else                     outFace = (stepZ > 0) ? 5 : 4;
                }
                outVoxel = v;
                return hitT;
            }
        }

        if (tmaxX < tmaxY) {
            if (tmaxX < tmaxZ) {
                if (tmaxX >= tMax) break;
                X += stepX; tmaxX += tdeltaX; lastAxis = 0;
                if (X < 0 || X >= (int)obj.sizeX) break;
            }
            else {
                if (tmaxZ >= tMax) break;
                Z += stepZ; tmaxZ += tdeltaZ; lastAxis = 2;
                if (Z < 0 || Z >= (int)obj.sizeZ) break;
            }
        }
        else {
            if (tmaxY < tmaxZ) {
                if (tmaxY >= tMax) break;
                Y += stepY; tmaxY += tdeltaY; lastAxis = 1;
                if (Y < 0 || Y >= (int)obj.sizeY) break;
            }
            else {
                if (tmaxZ >= tMax) break;
                Z += stepZ; tmaxZ += tdeltaZ; lastAxis = 2;
                if (Z < 0 || Z >= (int)obj.sizeZ) break;
            }
        }
    }
    return tMax;
}

bool Scene::TraceObjectDDAOcclusion(
    const float3& localO, const float3& localD,
    const VoxelObject& obj, float tMax) const
{
    int dummyFace;
    uint8_t dummyVoxel;
    return TraceObjectDDA(localO, localD, obj, tMax, dummyFace, dummyVoxel) < tMax;
}

void Scene::TraceInstanceBVH(
    Ray& ray, float& bestT,
    int& bestInstanceIdx, int& bestAxis, uint& bestVoxel) const
{
    using Node = tinybvh::BVH::BVHNode;

    tinybvh::Ray tbRay(
        { ray.O.x, ray.O.y, ray.O.z },
        { ray.D.x, ray.D.y, ray.D.z },
        bestT);

    const Node* node = &instanceBVH.bvhNode[0];
    const Node* stack[64];
    uint32_t stackPtr = 0;

    while (true)
    {
        if (node->isLeaf())
        {
            for (uint32_t i = 0; i < node->triCount; ++i)
            {
                const int si = (int)instanceBVH.primIdx[node->leftFirst + i];
                const VoxelInstance& inst = voxelInstances[si];
                if (inst.matricesDirty) continue;
                if (inst.modelIndex < 0 || inst.modelIndex >= (int)voxelObjects.size()) continue;

                float tEntry, tExit;
                if (!RayAABB(ray.O, ray.rD, inst.worldAABBmin, inst.worldAABBmax, tEntry, tExit))
                    continue;
                if (tEntry >= bestT) continue;

                const float3 localO = inst.worldToLocal.TransformPoint(ray.O);
                const float3 localD = inst.worldToLocal.TransformVector(ray.D);

                int hitFace; uint8_t hitVoxel;
                float t = TraceObjectDDA(localO, localD,
                    voxelObjects[inst.modelIndex], bestT, hitFace, hitVoxel);

                if (t < bestT)
                {
                    bestT = t;
                    bestInstanceIdx = si;
                    bestVoxel = hitVoxel;
                    tbRay.hit.t = bestT;

                    float bestDot = 1e30f;
                    for (int f = 0; f < 6; f++)
                    {
                        float d = dot(ray.D, inst.worldNormals[f]);
                        if (d < bestDot) { bestDot = d; bestAxis = f; }
                    }
                }

            }
            if (stackPtr == 0) break;
            node = stack[--stackPtr];
            continue;
        }

        const Node* child1 = &instanceBVH.bvhNode[node->leftFirst];
        const Node* child2 = &instanceBVH.bvhNode[node->leftFirst + 1];
        float dist1 = tinybvh::tinybvh_intersect_aabb(tbRay, child1->aabbMin, child1->aabbMax);
        float dist2 = tinybvh::tinybvh_intersect_aabb(tbRay, child2->aabbMin, child2->aabbMax);

        if (dist1 > dist2) {
            tinybvh::tinybvh_swap(dist1, dist2);
            tinybvh::tinybvh_swap(child1, child2);
        }
        if (dist1 == BVH_FAR) {
            if (stackPtr == 0) break;
            node = stack[--stackPtr];
        }
        else {
            node = child1;
            if (dist2 != BVH_FAR) stack[stackPtr++] = child2;
        }
    }
}

bool Scene::TraceInstanceBVHOcclusion(Ray& ray) const
{
    using Node = tinybvh::BVH::BVHNode;

    tinybvh::Ray tbRay(
        { ray.O.x, ray.O.y, ray.O.z },
        { ray.D.x, ray.D.y, ray.D.z },
        ray.t);

    const Node* node = &instanceBVH.bvhNode[0];
    const Node* stack[64];
    uint32_t stackPtr = 0;

    while (true)
    {
        if (node->isLeaf())
        {
            for (uint32_t i = 0; i < node->triCount; ++i)
            {
                const int si = (int)instanceBVH.primIdx[node->leftFirst + i];
                const VoxelInstance& inst = voxelInstances[si];
                if (inst.matricesDirty) continue;
                if (inst.modelIndex < 0 || inst.modelIndex >= (int)voxelObjects.size()) continue;

                float tEntry, tExit;
                if (!RayAABB(ray.O, ray.rD, inst.worldAABBmin, inst.worldAABBmax, tEntry, tExit))
                    continue;
                if (tEntry >= ray.t) continue;

                const float3 localO = inst.worldToLocal.TransformPoint(ray.O);
                const float3 localD = inst.worldToLocal.TransformVector(ray.D);

                if (TraceObjectDDAOcclusion(localO, localD,
                    voxelObjects[inst.modelIndex], ray.t))
                    return true;
            }
            if (stackPtr == 0) break;
            node = stack[--stackPtr];
            continue;
        }

        const Node* child1 = &instanceBVH.bvhNode[node->leftFirst];
        const Node* child2 = &instanceBVH.bvhNode[node->leftFirst + 1];
        float dist1 = tinybvh::tinybvh_intersect_aabb(tbRay, child1->aabbMin, child1->aabbMax);
        float dist2 = tinybvh::tinybvh_intersect_aabb(tbRay, child2->aabbMin, child2->aabbMax);

        if (dist1 > dist2) {
            tinybvh::tinybvh_swap(dist1, dist2);
            tinybvh::tinybvh_swap(child1, child2);
        }
        if (dist1 == BVH_FAR) {
            if (stackPtr == 0) break;
            node = stack[--stackPtr];
        }
        else {
            node = child1;
            if (dist2 != BVH_FAR) stack[stackPtr++] = child2;
        }
    }
    return false;
}



void Scene::FindNearest(Ray& ray) const
{
    float bestT = 1e34f;
    int   bestMatIdx = -1;
    int   bestAxis = -1;
    int   bestSphereIdx = -1;
    int   bestInstanceIdx = -1;
    uint  bestVoxel = 0;

    float nearestSphereT = 1e34f;
    int   nearestSphereIdx = -1;
    if (sphereSOA.count > 0)
    {
        if (useLegacyBVH && sphereBVHReady)
        {
            tinybvh::Ray bvhRay({ ray.O.x, ray.O.y, ray.O.z },
                { ray.D.x, ray.D.y, ray.D.z });
            TraceSphereBVH(bvhRay);
            if (bvhRay.hit.t < 1e30f)
            {
                nearestSphereT = bvhRay.hit.t;
                nearestSphereIdx = (int)bvhRay.hit.prim;
            }
        }
        else if (sphereGrid.ready)
        {
            TraceSphereGrid(ray, nearestSphereT, nearestSphereIdx);
        }
    }
    if (nearestSphereIdx >= 0 && nearestSphereT < bestT)
    {
        bestT = nearestSphereT;
        bestMatIdx = sphereSOA.material[nearestSphereIdx];
        bestAxis = 3;
        bestSphereIdx = nearestSphereIdx;
        bestInstanceIdx = -1;
        bestVoxel = 0;
    }

    if (voxelGridActive)
    {
        uint hitMat = 0;
        int  hitAxis = -1;
        ray.t = 1e34f;
        if (TraverseDDA<false>(ray, bestT, hitMat, hitAxis) && ray.t < bestT)
        {
            bestT = ray.t;
            bestMatIdx = hitMat;
            bestAxis = hitAxis;
            bestSphereIdx = -1;
            bestInstanceIdx = -1;
            bestVoxel = hitMat;
        }
    }

    if (!voxelInstances.empty())
    {
        int  instIdx = -1;
        int  instAxis = -1;
        uint instVoxel = 0;

        if (instanceBVHReady)
        {
            TraceInstanceBVH(ray, bestT, instIdx, instAxis, instVoxel);
        }
        else
        {
            for (int i = 0; i < (int)voxelInstances.size(); i++)
            {
                const VoxelInstance& inst = voxelInstances[i];
                if (inst.matricesDirty) continue;
                if (inst.modelIndex < 0 ||
                    inst.modelIndex >= (int)voxelObjects.size()) continue;

                float tEntry, tExit;
                if (!RayAABB(ray.O, ray.rD,
                    inst.worldAABBmin, inst.worldAABBmax,
                    tEntry, tExit)) continue;
                if (tEntry >= bestT) continue;

                const float3 localO = inst.worldToLocal.TransformPoint(ray.O);
                const float3 localD = inst.worldToLocal.TransformVector(ray.D);

                int hitFace; uint8_t hitVoxel;
                float t = TraceObjectDDA(localO, localD,
                    voxelObjects[inst.modelIndex], bestT, hitFace, hitVoxel);
                if (t < bestT)
                {
                    bestT = t;
                    instIdx = i;
                    instAxis = hitFace;
                    instVoxel = hitVoxel;
                }
            }
        }

        if (instIdx >= 0)
        {
            bestMatIdx = instVoxel;
            bestAxis = instAxis;
            bestSphereIdx = -1;
            bestInstanceIdx = instIdx;
            bestVoxel = instVoxel;
        }
    }

    ray.t = bestT;
    ray.materialIndex = bestMatIdx;
    ray.axis = bestAxis;
    ray.sphereIndex = bestSphereIdx;
    ray.instanceIndex = bestInstanceIdx;
    ray.voxel = bestVoxel;
}

bool Scene::IsOccluded(Ray& ray) const
{
    if (voxelGridActive)
    {
        uint dummyMat = 0;
        int  dummyAxis = -1;
        if (TraverseDDA<true>(ray, 0.f, dummyMat, dummyAxis))
            return true;
    }

    if (instancesShadows && !voxelInstances.empty())
    {
        if (instanceBVHReady)
        {
            if (TraceInstanceBVHOcclusion(ray))
                return true;
        }
        else
        {
            for (int i = 0; i < (int)voxelInstances.size(); i++)
            {
                const VoxelInstance& inst = voxelInstances[i];
                if (inst.matricesDirty) continue;
                if (inst.modelIndex < 0 ||
                    inst.modelIndex >= (int)voxelObjects.size()) continue;

                float tEntry, tExit;
                if (!RayAABB(ray.O, ray.rD,
                    inst.worldAABBmin, inst.worldAABBmax,
                    tEntry, tExit)) continue;
                if (tEntry >= ray.t) continue;

                const float3 localO = inst.worldToLocal.TransformPoint(ray.O);
                const float3 localD = inst.worldToLocal.TransformVector(ray.D);

                if (TraceObjectDDAOcclusion(localO, localD,
                    voxelObjects[inst.modelIndex], ray.t))
                    return true;
            }
        }
    }

    return false;
}
