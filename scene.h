#pragma once

// high level settings
#define WORLDSIZE 128 // power of 2. Warning: max 512 for a 512x512x512x4 bytes = 512MB world!
#define GRIDSIZE  WORLDSIZE              // 128
#define GRIDSIZE2 WORLDSIZE * WORLDSIZE  // 16,384  (one XZ slice)
#define GRIDSIZE3 WORLDSIZE * WORLDSIZE * WORLDSIZE  // 2,097,152 (whole grid)

// low-level / derived
#define WORLDSIZE2	(WORLDSIZE*WORLDSIZE)
#define WORLDSIZE3	(WORLDSIZE*WORLDSIZE*WORLDSIZE)

// epsilon
#define EPSILON		0.00001f

enum MaterialID : uint8_t
{
    MAT_NONE = 0,             // empty voxel
    MAT_LAMBERTIAN = 1,       // Default Lambertian
    MAT_MIRROR = 2,           // Mirror material
    MAT_DIELECTRIC = 3,       // Glass material
    MAT_LAMBERTIAN_WHITE = 4,
    MAT_LAMBERTIAN_GRAY = 5,
    MAT_RED = 6,
    MAT_GREEN = 7,
    MAT_BLUE = 8,
    MAT_COUNT
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

		//claude
		static void SaveGrid(const uint* grid, const char* filename) {
			std::ofstream file(filename, std::ios::binary);
			file.write(reinterpret_cast<const char*>(grid), GRIDSIZE3 * sizeof(uint));
		}
		//claude
		static void LoadGrid(uint* grid, const char* filename) {
			std::ifstream file(filename, std::ios::binary);
			file.read(reinterpret_cast<char*>(grid), GRIDSIZE3 * sizeof(uint));
		}
	};

}
