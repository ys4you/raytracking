#include "template.h"

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

    // Lambertian base (optional)
    materials[MAT_LAMBERTIAN].type = MaterialType::Lambertian;
    materials[MAT_LAMBERTIAN].albedo = { 0.8f, 0.7f, 0.6f };

    // Mirror
    materials[MAT_MIRROR].type = MaterialType::Metal;
    materials[MAT_MIRROR].albedo = { 0.9f, 0.9f, 0.95f };
    materials[MAT_MIRROR].roughness = 0.05f;
    materials[MAT_MIRROR].metallic = 1.0f;

    // Dielectric
    materials[MAT_DIELECTRIC].type = MaterialType::Dielectric;
    materials[MAT_DIELECTRIC].albedo = { 1.0f, 1.0f, 1.0f };
    materials[MAT_DIELECTRIC].ior = 1.5f;


    ///TestScene
    // Checkerboard floor
    materials[MAT_LAMBERTIAN_WHITE].type = MaterialType::Lambertian;
    materials[MAT_LAMBERTIAN_WHITE].albedo = { 0.9f, 0.9f, 0.9f };

    materials[MAT_LAMBERTIAN_GRAY].type = MaterialType::Lambertian;
    materials[MAT_LAMBERTIAN_GRAY].albedo = { 0.3f, 0.3f, 0.3f };

    // Colored cubes
    materials[MAT_RED].type = MaterialType::Lambertian;
    materials[MAT_RED].albedo = { 1.0f, 0.26f, 0.26f };

    materials[MAT_GREEN].type = MaterialType::Lambertian;
    materials[MAT_GREEN].albedo = { 0.26f, 1.0f, 0.26f };

    materials[MAT_BLUE].type = MaterialType::Lambertian;
    materials[MAT_BLUE].albedo = { 0.26f, 0.26f, 1.0f };


    materials[MAT_ORANGE].type = MaterialType::Lambertian;
    materials[MAT_ORANGE].albedo = { 1.0f, 0.55f, 0.10f };

    materials[MAT_YELLOW].type = MaterialType::Lambertian;
    materials[MAT_YELLOW].albedo = { 1.0f, 0.90f, 0.10f };

    materials[MAT_PURPLE].type = MaterialType::Lambertian;
    materials[MAT_PURPLE].albedo = { 0.55f, 0.10f, 0.90f };

    materials[MAT_CYAN].type = MaterialType::Lambertian;
    materials[MAT_CYAN].albedo = { 0.10f, 0.85f, 0.85f };

    materials[MAT_BROWN].type = MaterialType::Lambertian;
    materials[MAT_BROWN].albedo = { 0.45f, 0.28f, 0.12f };

    materials[MAT_TEAL].type = MaterialType::Lambertian;
    materials[MAT_TEAL].albedo = { 0.10f, 0.55f, 0.50f };
    ///TestScene EOD

    const bool GENERATE = false; 

    if (GENERATE)
    {

    }
    else
    {
        //LoadGrid(grid, "assets/ReproScene.bin");
        LoadGrid(grid, "assets/TestScene.bin");
    }


}

void Scene::Set(const uint x, const uint y, const uint z, const uint v)
{
	grid[x + y * WORLDSIZE + z * WORLDSIZE2] = v;
}

bool Scene::Setup3DDDA(Ray& ray, DDAState& state) const
{
	// if ray is not inside the world: advance until it is
	state.t = 0;
	bool startedInGrid = point_in_cube(ray.O);
	if (!startedInGrid)
	{
		state.t = intersect_cube(ray);
		if (state.t > 1e33f) return false; // ray misses voxel data entirely
	}
	// setup amanatides & woo - assume world is 1x1x1, from (0,0,0) to (1,1,1)
	static const float cellSize = 1.0f / WORLDSIZE;
	state.step = make_int3(1.0f - ray.Dsign * 2.0f);
	const float3 posInGrid = (float)WORLDSIZE * (ray.O + (state.t + 0.00005f) * ray.D);
	const float3 gridPlanes = (ceilf(posInGrid) - ray.Dsign) * cellSize;
	const int3 P = clamp(make_int3(posInGrid), 0, WORLDSIZE - 1);
	state.X = P.x, state.Y = P.y, state.Z = P.z;
	state.tdelta = cellSize * float3(state.step) * ray.rD;
	state.tmax = (gridPlanes - ray.O) * ray.rD;
	// detect rays that start inside a voxel
	uint cell = grid[P.x + P.y * WORLDSIZE + P.z * WORLDSIZE2];
	ray.inside = cell != 0 && startedInGrid;
	// proceed with traversal
	return true;
}

void Scene::FindNearest(Ray& ray) const
{
    // Nudge origin to avoid self-intersection
    ray.O += EPSILON * ray.D;

    // Setup Amanatides & Woo 3D DDA
    DDAState s;
    if (!Setup3DDDA(ray, s)) return;

    uint cell = 0;
    uint lastCell = 0;
    uint axis = ray.axis;

    if (ray.inside)
    {
        // Ray starts inside a filled voxel, step until we find empty space
        while (true)
        {
            cell = grid[s.X + s.Y * WORLDSIZE + s.Z * WORLDSIZE2];
            if (!cell) break; // found empty space

            lastCell = cell;

            // Advance to next voxel
            if (s.tmax.x < s.tmax.y)
            {
                if (s.tmax.x < s.tmax.z)
                {
                    s.t = s.tmax.x;
                    s.X += s.step.x;
                    axis = 0;
                    if (s.X >= WORLDSIZE) break;
                    s.tmax.x += s.tdelta.x;
                }
                else
                {
                    s.t = s.tmax.z;
                    s.Z += s.step.z;
                    axis = 2;
                    if (s.Z >= WORLDSIZE) break;
                    s.tmax.z += s.tdelta.z;
                }
            }
            else
            {
                if (s.tmax.y < s.tmax.z)
                {
                    s.t = s.tmax.y;
                    s.Y += s.step.y;
                    axis = 1;
                    if (s.Y >= WORLDSIZE) break;
                    s.tmax.y += s.tdelta.y;
                }
                else
                {
                    s.t = s.tmax.z;
                    s.Z += s.step.z;
                    axis = 2;
                    if (s.Z >= WORLDSIZE) break;
                    s.tmax.z += s.tdelta.z;
                }
            }
        }

        ray.voxel = lastCell;

        // Assign material of the voxel we just left
        if (ray.voxel != 0)
        {
            int hitX = s.X - s.step.x;
            int hitY = s.Y - s.step.y;
            int hitZ = s.Z - s.step.z;

            // Bounds check before accessing grid
            if (hitX >= 0 && hitX < WORLDSIZE &&
                hitY >= 0 && hitY < WORLDSIZE &&
                hitZ >= 0 && hitZ < WORLDSIZE)
            {
                int idx = hitX + hitY * WORLDSIZE + hitZ * WORLDSIZE2;
                ray.materialIndex = grid[idx];
            }
            else
            {
                ray.materialIndex = 0; // or default material
            }
        }
    }
    else
    {
        // Ray starts outside, step until we hit a filled voxel
        while (true)
        {
            cell = grid[s.X + s.Y * WORLDSIZE + s.Z * WORLDSIZE2];
            if (cell) break; // hit voxel

            // Advance to next voxel
            if (s.tmax.x < s.tmax.y)
            {
                if (s.tmax.x < s.tmax.z)
                {
                    s.t = s.tmax.x;
                    s.X += s.step.x;
                    axis = 0;
                    if (s.X >= WORLDSIZE) return;
                    s.tmax.x += s.tdelta.x;
                }
                else
                {
                    s.t = s.tmax.z;
                    s.Z += s.step.z;
                    axis = 2;
                    if (s.Z >= WORLDSIZE) return;
                    s.tmax.z += s.tdelta.z;
                }
            }
            else
            {
                if (s.tmax.y < s.tmax.z)
                {
                    s.t = s.tmax.y;
                    s.Y += s.step.y;
                    axis = 1;
                    if (s.Y >= WORLDSIZE) return;
                    s.tmax.y += s.tdelta.y;
                }
                else
                {
                    s.t = s.tmax.z;
                    s.Z += s.step.z;
                    axis = 2;
                    if (s.Z >= WORLDSIZE) return;
                    s.tmax.z += s.tdelta.z;
                }
            }
        }

        ray.voxel = cell;

        if (ray.voxel != 0)
        {
            int idx = s.X + s.Y * WORLDSIZE + s.Z * WORLDSIZE2;
            ray.materialIndex = grid[idx];
        }
    }

    ray.t = s.t;
    ray.axis = axis;
}

bool Scene::IsOccluded(Ray& ray) const
{
	// nudge origin
	ray.O += EPSILON * ray.D;
	ray.t -= EPSILON * 2.0f;
	// setup Amanatides & Woo grid traversal
	DDAState s;
	if (!Setup3DDDA(ray, s)) return false;
	// start stepping
	while (s.t < ray.t)
	{
		const uint cell = grid[s.X + s.Y * WORLDSIZE + s.Z * WORLDSIZE2];
		if (cell) /* we hit a solid voxel */ return s.t < ray.t;
		if (s.tmax.x < s.tmax.y)
		{
			if (s.tmax.x < s.tmax.z) { if ((s.X += s.step.x) >= WORLDSIZE) return false; s.t = s.tmax.x, s.tmax.x += s.tdelta.x; }
			else { if ((s.Z += s.step.z) >= WORLDSIZE) return false; s.t = s.tmax.z, s.tmax.z += s.tdelta.z; }
		}
		else
		{
			if (s.tmax.y < s.tmax.z) { if ((s.Y += s.step.y) >= WORLDSIZE) return false; s.t = s.tmax.y, s.tmax.y += s.tdelta.y; }
			else { if ((s.Z += s.step.z) >= WORLDSIZE) return false; s.t = s.tmax.z, s.tmax.z += s.tdelta.z; }
		}
	}
	return false;
}