#pragma once

// high level settings
#define WORLDSIZE 128 // power of 2. Warning: max 512 for a 512x512x512x4 bytes = 512MB world!

// low-level / derived
#define WORLDSIZE2	(WORLDSIZE*WORLDSIZE)
#define WORLDSIZE3	(WORLDSIZE*WORLDSIZE*WORLDSIZE)

// epsilon
#define EPSILON		0.00001f

enum MaterialID : uint8_t
{
	MAT_LAMBERTIAN = 0,       // Default Lambertian
	MAT_MIRROR = 1,           // Mirror material
	MAT_DIELECTRIC = 2,       // Glass material

	// Checkerboard floor
	MAT_LAMBERTIAN_WHITE = 3,
	MAT_LAMBERTIAN_GRAY = 4,

	// Colored cubes
	MAT_RED = 5,
	MAT_GREEN = 6,
	MAT_BLUE = 7,

	MAT_COUNT                 // Total number of materials
};



struct Material;

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
		};
		Scene();
		void FindNearest(Ray& ray) const;
		bool IsOccluded(Ray& ray) const;
		void Set(const uint x, const uint y, const uint z, const uint v);
		unsigned int* grid; // voxel payload is 'unsigned int', interpretation of the bits is free!
		std::array<Material, MAT_COUNT> materials;

	private:
		bool Setup3DDDA(Ray& ray, DDAState& state) const;
	};

}
