#include "template.h"

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

Scene::Scene()
{
	topGrid = new uint[TOP_SIZE3]();
	bricks = new Brick[TOP_SIZE3];
	brickCount = 0;

#pragma omp parallel for schedule(dynamic)
	for (int z = 0; z < WORLDSIZE; z++)
	{
		const float fz = (float)z / WORLDSIZE;
		for (int y = 0; y < WORLDSIZE; y++)
		{
			float fx = 0, fy = (float)y / WORLDSIZE;
			for (int x = 0; x < WORLDSIZE; x++, fx += 1.0f / WORLDSIZE)
			{
				const float n = noise3D(fx, fy, fz);
				Set(x, y, z, n > 0.09f ? 0x020101 * y : 0);
			}
		}
	}

	printf("Bricks created: %u out of %u possible\n", brickCount, TOP_SIZE3);
}

Scene::~Scene()
{
	delete[] topGrid;
	delete[] bricks;
}

void Scene::Set(const uint x, const uint y, const uint z, const uint v)
{
	uint topX = x >> BRICK_SHIFT;
	uint topY = y >> BRICK_SHIFT;
	uint topZ = z >> BRICK_SHIFT;

	uint localX = x & (BRICK_SIZE - 1);
	uint localY = y & (BRICK_SIZE - 1);
	uint localZ = z & (BRICK_SIZE - 1);

	uint topIdx = topX + topY * TOP_SIZE + topZ * TOP_SIZE2;
	uint localIdx = localX + localY * BRICK_SIZE + localZ * BRICK_SIZE2;

	uint brickRef = topGrid[topIdx];
	if (brickRef == 0 && v != 0)
	{
		brickRef = ++brickCount;
		topGrid[topIdx] = brickRef;
		memset(&bricks[brickRef - 1], 0, sizeof(Brick));
	}

	if (brickRef != 0)
	{
		bricks[brickRef - 1].voxels[localIdx] = v;
	}
}

uint Scene::Get(const uint x, const uint y, const uint z) const
{
	uint topX = x >> BRICK_SHIFT;
	uint topY = y >> BRICK_SHIFT;
	uint topZ = z >> BRICK_SHIFT;
	uint topIdx = topX + topY * TOP_SIZE + topZ * TOP_SIZE2;
	uint brickRef = topGrid[topIdx];

	if (brickRef == 0) return 0;

	uint localX = x & (BRICK_SIZE - 1);
	uint localY = y & (BRICK_SIZE - 1);
	uint localZ = z & (BRICK_SIZE - 1);
	uint localIdx = localX + localY * BRICK_SIZE + localZ * BRICK_SIZE2;

	return bricks[brickRef - 1].voxels[localIdx];
}

// Setup DDA for any grid size (top-level or brick-level)
bool Scene::Setup3DDDA(Ray& ray, DDAState& state, int gridSize, float cellSize) const
{
	state.t = 0;
	bool startedInGrid = point_in_cube(ray.O);

	if (!startedInGrid)
	{
		state.t = intersect_cube(ray);
		if (state.t > 1e33f) return false;
	}

	state.step = make_int3(1.0f - ray.Dsign * 2.0f);

	const float3 posInGrid = (float)gridSize * (ray.O + (state.t + 0.00005f) * ray.D);
	const float3 gridPlanes = (ceilf(posInGrid) - ray.Dsign) * cellSize;
	const int3 P = clamp(make_int3(posInGrid), 0, gridSize - 1);

	state.X = P.x;
	state.Y = P.y;
	state.Z = P.z;
	state.tdelta = cellSize * float3(state.step) * ray.rD;
	state.tmax = (gridPlanes - ray.O) * ray.rD;

	int gridSize2 = gridSize * gridSize;
	state.gridIdx = P.x + P.y * gridSize + P.z * gridSize2;
	state.stepIdx[0] = state.step.x;
	state.stepIdx[1] = state.step.y * gridSize;
	state.stepIdx[2] = state.step.z * gridSize2;

	return true;
}

// Search inside a brick for voxels
bool Scene::TraverseBrick(Ray& ray, uint brickIdx, const DDAState& topState, uint& axis) const
{
	const Brick& brick = bricks[brickIdx];

	// Calculate brick bounds in world space [0,1]
	float invTopSize = 1.0f / TOP_SIZE;
	float3 brickMin = float3(topState.X, topState.Y, topState.Z) * invTopSize;
	float3 brickMax = brickMin + float3(invTopSize);

	// Find where ray enters this brick
	float3 invDir = ray.rD;
	float3 t0 = (brickMin - ray.O) * invDir;
	float3 t1 = (brickMax - ray.O) * invDir;
	float3 tmin3 = fminf(t0, t1);
	float3 tmax3 = fmaxf(t0, t1);

	float tEnter = max(max(max(tmin3.x, tmin3.y), tmin3.z), topState.t);
	float tExit = min(min(tmax3.x, tmax3.y), tmax3.z);

	if (tEnter > tExit) return false;

	// Convert entry point to local voxel coordinates [0,8)
	float3 entryPos = ray.O + (tEnter + 0.0001f) * ray.D;
	float3 localPos = (entryPos - brickMin) * (float)TOP_SIZE * (float)BRICK_SIZE;
	int3 voxelPos = clamp(make_int3(localPos), 0, BRICK_SIZE - 1);

	// Setup voxel-level DDA
	int3 step = make_int3(1.0f - ray.Dsign * 2.0f);
	float cellSize = 1.0f / WORLDSIZE;

	float3 voxelMin = brickMin + float3(voxelPos) * cellSize;
	float3 voxelMax = voxelMin + float3(cellSize);

	float3 tMax;
	tMax.x = ((ray.D.x >= 0 ? voxelMax.x : voxelMin.x) - ray.O.x) * invDir.x;
	tMax.y = ((ray.D.y >= 0 ? voxelMax.y : voxelMin.y) - ray.O.y) * invDir.y;
	tMax.z = ((ray.D.z >= 0 ? voxelMax.z : voxelMin.z) - ray.O.z) * invDir.z;

	float3 tDelta = float3(cellSize) * float3(step) * invDir;

	int gridIdx = voxelPos.x + voxelPos.y * BRICK_SIZE + voxelPos.z * BRICK_SIZE2;
	int stepIdx[3] = { step.x, step.y * BRICK_SIZE, step.z * BRICK_SIZE2 };

	float t = tEnter;

	// DDA through voxels in brick
	while (voxelPos.x >= 0 && voxelPos.x < BRICK_SIZE &&
		voxelPos.y >= 0 && voxelPos.y < BRICK_SIZE &&
		voxelPos.z >= 0 && voxelPos.z < BRICK_SIZE)
	{
		uint voxel = brick.voxels[gridIdx];

		if (voxel != 0)
		{
			ray.voxel = voxel;
			ray.t = t;
			ray.axis = axis;
			return true;
		}

		// Step to next voxel
		if (tMax.x < tMax.y)
		{
			if (tMax.x < tMax.z)
			{
				t = tMax.x; voxelPos.x += step.x; gridIdx += stepIdx[0]; axis = 0; tMax.x += tDelta.x;
			}
			else
			{
				t = tMax.z; voxelPos.z += step.z; gridIdx += stepIdx[2]; axis = 2; tMax.z += tDelta.z;
			}
		}
		else
		{
			if (tMax.y < tMax.z)
			{
				t = tMax.y; voxelPos.y += step.y; gridIdx += stepIdx[1]; axis = 1; tMax.y += tDelta.y;
			}
			else
			{
				t = tMax.z; voxelPos.z += step.z; gridIdx += stepIdx[2]; axis = 2; tMax.z += tDelta.z;
			}
		}
	}

	return false;
}

// Main traversal: DDA through top-level grid, search bricks when found
void Scene::FindNearest(Ray& ray) const
{
	ray.O += EPSILON * ray.D;
	ray.voxel = 0;
	ray.t = 1e34f;

	DDAState topState;
	if (!Setup3DDDA(ray, topState, TOP_SIZE, 1.0f / TOP_SIZE))
		return;

	uint axis = 0;

	while (topState.X >= 0 && topState.X < TOP_SIZE &&
		topState.Y >= 0 && topState.Y < TOP_SIZE &&
		topState.Z >= 0 && topState.Z < TOP_SIZE)
	{
		uint brickRef = topGrid[topState.gridIdx];

		if (brickRef != 0)
		{
			if (TraverseBrick(ray, brickRef - 1, topState, axis))
				return;
		}

		// Step to next top-level cell
		if (topState.tmax.x < topState.tmax.y)
		{
			if (topState.tmax.x < topState.tmax.z)
			{
				topState.t = topState.tmax.x; topState.X += topState.step.x; topState.gridIdx += topState.stepIdx[0]; axis = 0; topState.tmax.x += topState.tdelta.x;
			}
			else
			{
				topState.t = topState.tmax.z; topState.Z += topState.step.z; topState.gridIdx += topState.stepIdx[2]; axis = 2; topState.tmax.z += topState.tdelta.z;
			}
		}
		else
		{
			if (topState.tmax.y < topState.tmax.z)
			{
				topState.t = topState.tmax.y; topState.Y += topState.step.y; topState.gridIdx += topState.stepIdx[1]; axis = 1; topState.tmax.y += topState.tdelta.y;
			}
			else
			{
				topState.t = topState.tmax.z; topState.Z += topState.step.z; topState.gridIdx += topState.stepIdx[2]; axis = 2; topState.tmax.z += topState.tdelta.z;
			}
		}
	}
}

// Shadow ray: same as FindNearest but stops early if anything is hit
bool Scene::IsOccluded(Ray& ray) const
{
	ray.O += EPSILON * ray.D;
	float maxT = ray.t - EPSILON * 2.0f;

	DDAState topState;
	if (!Setup3DDDA(ray, topState, TOP_SIZE, 1.0f / TOP_SIZE))
		return false;

	uint axis = 0;

	while (topState.t < maxT &&
		topState.X >= 0 && topState.X < TOP_SIZE &&
		topState.Y >= 0 && topState.Y < TOP_SIZE &&
		topState.Z >= 0 && topState.Z < TOP_SIZE)
	{
		uint brickRef = topGrid[topState.gridIdx];

		if (brickRef != 0)
		{
			if (TraverseBrick(ray, brickRef - 1, topState, axis))
			{
				if (ray.t < maxT) return true;
			}
		}

		if (topState.tmax.x < topState.tmax.y)
		{
			if (topState.tmax.x < topState.tmax.z)
			{
				topState.t = topState.tmax.x; topState.X += topState.step.x; topState.gridIdx += topState.stepIdx[0]; axis = 0; topState.tmax.x += topState.tdelta.x;
			}
			else
			{
				topState.t = topState.tmax.z; topState.Z += topState.step.z; topState.gridIdx += topState.stepIdx[2]; axis = 2; topState.tmax.z += topState.tdelta.z;
			}
		}
		else
		{
			if (topState.tmax.y < topState.tmax.z)
			{
				topState.t = topState.tmax.y; topState.Y += topState.step.y; topState.gridIdx += topState.stepIdx[1]; axis = 1; topState.tmax.y += topState.tdelta.y;
			}
			else
			{
				topState.t = topState.tmax.z; topState.Z += topState.step.z; topState.gridIdx += topState.stepIdx[2]; axis = 2; topState.tmax.z += topState.tdelta.z;
			}
		}
	}

	return false;
}