#include "template.h"

#include "VoxLoader.h"
#include "Core/Material.h"

#include "Core/Material.h"

inline float intersect_cube(Ray& ray)
{
	// branchless slab method by Tavian
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
	// test if pos is inside the cube
	return pos.x >= 0 && pos.y >= 0 && pos.z >= 0 &&
		pos.x <= 1 && pos.y <= 1 && pos.z <= 1;
}

Scene::Scene()
{
    grid = (uint*)MALLOC64(WORLDSIZE3 * sizeof(uint));
    memset(grid, 0, WORLDSIZE3 * sizeof(uint));

    // Initialize materials
    materials.fill(Material{});

    // Mirror
    materials[MAT_MIRROR].type = MaterialType::Metal;
    materials[MAT_MIRROR].albedo = { 0.9f, 0.9f, 0.95f };
    materials[MAT_MIRROR].roughness = 0.05f;
    materials[MAT_MIRROR].metallic = 1.0f;

    // Dielectric
    materials[MAT_DIELECTRIC].type = MaterialType::Dielectric;
    materials[MAT_DIELECTRIC].albedo = { 1.0f, 1.0f, 1.0f };
    materials[MAT_DIELECTRIC].ior = 1.5f;

    materials[MAT_GREEN].type = MaterialType::Lambertian;
    materials[MAT_GREEN].albedo = { 0.0f, 1.0f, 0.0f };
    materials[MAT_GREEN].roughness = 1.0f;

    spheres.push_back
	({
	    float3(0.5f, 0.4f, 0.5f),
	    0.1f,
        MAT_GREEN
	});

    //loading the map
    {
        //LoadGrid(grid, "assets/ReproScene.bin");
        //LoadGrid(grid, "assets/TestScene.bin");
    	VoxLoader::Load("assets/street.vox", grid, GRIDSIZE, materials.data());
    }


}

void Scene::Set(const uint x, const uint y, const uint z, const uint v)
{
	grid[x + y * WORLDSIZE + z * WORLDSIZE2] = v;
}
bool IntersectSphere(const Ray& ray, const Sphere& s, float& tHit)
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


bool Scene::Setup3DDDA(Ray& ray, DDAState& state) const
{
    state.t = 0;
    bool startedInGrid = point_in_cube(ray.O);
    if (!startedInGrid)
    {
        state.t = intersect_cube(ray);
        if (state.t > 1e33f) return false; // ray misses voxel data entirely
    }

    static const float cellSize = 1.0f / WORLDSIZE;

    // Step: +1 or -1
    state.step = make_int3(
        1 - ray.Dsign.x * 2,
        1 - ray.Dsign.y * 2,
        1 - ray.Dsign.z * 2
    );
    // Position in grid (component-wise)
    float3 posInGrid;
    posInGrid.x = (ray.O.x + (state.t + 0.00005f) * ray.D.x) * WORLDSIZE;
    posInGrid.y = (ray.O.y + (state.t + 0.00005f) * ray.D.y) * WORLDSIZE;
    posInGrid.z = (ray.O.z + (state.t + 0.00005f) * ray.D.z) * WORLDSIZE;

    // Grid planes (component-wise)
    float3 gridPlanes;
    gridPlanes.x = (ceilf(posInGrid.x) - ray.Dsign.x) * cellSize;
    gridPlanes.y = (ceilf(posInGrid.y) - ray.Dsign.y) * cellSize;
    gridPlanes.z = (ceilf(posInGrid.z) - ray.Dsign.z) * cellSize;

    // Clamp to voxel coordinates
    int3 P;
    P.x = clamp(int(posInGrid.x), 0, WORLDSIZE - 1);
    P.y = clamp(int(posInGrid.y), 0, WORLDSIZE - 1);
    P.z = clamp(int(posInGrid.z), 0, WORLDSIZE - 1);

    state.X = P.x;
    state.Y = P.y;
    state.Z = P.z;

    // tdelta = cellSize * step / ray.D (component-wise)
    state.tdelta.x = cellSize * state.step.x / ray.D.x;
    state.tdelta.y = cellSize * state.step.y / ray.D.y;
    state.tdelta.z = cellSize * state.step.z / ray.D.z;

    // tmax = (gridPlanes - origin) / ray.D
    state.tmax.x = (gridPlanes.x - ray.O.x) / ray.D.x;
    state.tmax.y = (gridPlanes.y - ray.O.y) / ray.D.y;
    state.tmax.z = (gridPlanes.z - ray.O.z) / ray.D.z;

    // Detect if ray starts inside a voxel
    uint cell = grid[state.X + state.Y * WORLDSIZE + state.Z * WORLDSIZE2];
    ray.inside = cell != 0 && startedInGrid;

    return true;
}

void Scene::FindNearest(Ray& ray) const
{
    ray.O += EPSILON * ray.D;

    float nearestSphereT = 1e34f;
    int nearestSphereIdx = -1;
    for (int i = 0; i < spheres.size(); i++)
    {
        float t;
        if (IntersectSphere(ray, spheres[i], t) && t < nearestSphereT)
        {
            nearestSphereT = t;
            nearestSphereIdx = i;
        }
    }

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
            cell = grid[s.X + s.Y * WORLDSIZE + s.Z * WORLDSIZE2];

            if ((startedInside && cell == 0) || (!startedInside && cell != 0))
            {
                nearestVoxelT = s.t;
                voxelHitAxis = -1;

                if (startedInside)
                {
                    int hitX = s.X - s.step.x;
                    int hitY = s.Y - s.step.y;
                    int hitZ = s.Z - s.step.z;
                    if (hitX >= 0 && hitX < WORLDSIZE &&
                        hitY >= 0 && hitY < WORLDSIZE &&
                        hitZ >= 0 && hitZ < WORLDSIZE)
                    {
                        int idx = hitX + hitY * WORLDSIZE + hitZ * WORLDSIZE2;
                        hitVoxelMaterial = grid[idx];
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

            // Out of bounds
            if (s.X < 0 || s.X >= WORLDSIZE ||
                s.Y < 0 || s.Y >= WORLDSIZE ||
                s.Z < 0 || s.Z >= WORLDSIZE)
            {
                break;
            }
        }
    }

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
        // Voxel is closer
        ray.t = nearestVoxelT;
        ray.materialIndex = hitVoxelMaterial;
        ray.axis = voxelHitAxis;
        ray.sphereIndex = -1;
        ray.voxel = hitVoxelMaterial;
    }
    else
    {
        // Nothing hit
        ray.t = 1e34f;
        ray.materialIndex = -1;
        ray.axis = -1;
        ray.sphereIndex = -1;
        ray.voxel = 0;
    }
}
bool Scene::IsOccluded(Ray& ray) const
{
    ray.O += EPSILON * ray.D;
    ray.t -= EPSILON * 2.0f;

    DDAState s;
    if (!Setup3DDDA(ray, s)) return false;

    while (s.t < ray.t)
    {
        const uint cell = grid[s.X + s.Y * WORLDSIZE + s.Z * WORLDSIZE2];
        if (cell) return true; // hit voxel

        // Step to next voxel
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