#pragma once

// high level settings
#define WORLDSIZE 128 // power of 2. Warning: max 512 for a 512x512x512x4 bytes = 512MB world!

// low-level / derived
#define WORLDSIZE2	(WORLDSIZE*WORLDSIZE)
#define WORLDSIZE3	(WORLDSIZE*WORLDSIZE*WORLDSIZE)

// epsilon
#define EPSILON		0.00001f


#define BRICK_SIZE 8		//each brick holds 8 x 8 x 8 voxels
#define BRICK_SHIFT 3		// log2(8) = 3, used for the fast devision
#define TOP_SIZE (WORLDSIZE / BRICK_SIZE)	//128/8 = 16
#define TOP_SIZE2   (TOP_SIZE * TOP_SIZE)       // 256
#define TOP_SIZE3   (TOP_SIZE * TOP_SIZE * TOP_SIZE)  // 4096
#define BRICK_SIZE2 (BRICK_SIZE * BRICK_SIZE)   // 64
#define BRICK_SIZE3 (BRICK_SIZE * BRICK_SIZE * BRICK_SIZE)  // 512



struct Brick
{
	uint voxels[BRICK_SIZE * BRICK_SIZE * BRICK_SIZE];  // 512 voxels
};


namespace Tmpl8 {

class Scene
{
public:
	struct DDAState
	{
		int3 step;
		uint X, Y, Z;
		float t;
		float3 tdelta;
		float3 tmax;

		uint gridIdx;	//current pos in grid array
		int stepIdx[3];	// index increments for X,Y,Z steps 
	};


	Scene();
	~Scene();
	void FindNearest( Ray& ray ) const;
	bool IsOccluded( Ray& ray ) const;
	void Set( const uint x, const uint y, const uint z, const uint v );
	uint Get(const uint x, const uint y, const uint z) const;


	uint* topGrid;      // TOP_SIZE = 4096 entries
	Brick* bricks;      // Array of bricks
	uint brickCount;    // Number of allocated bricks


private:
	bool Setup3DDDA( Ray& ray, DDAState& state, int gridSize, float cellSize ) const;
	bool TraverseBrick(Ray& ray, uint brickIdx, const DDAState& topState, uint& axis) const;

};

}