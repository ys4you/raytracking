#include "template.h"

#include "Core/TextureTable.h"

Ray::Ray( const float3 origin, const float3 direction, const float rayLength, const uint rgb )
	: O( origin ), D( normalize( direction ) ), t( rayLength ), voxel( rgb )
{
	// calculate reciprocal ray direction for triangles and AABBs
	// TODO: prevent NaNs - or don't
	rD = float3( 1 / D.x, 1 / D.y, 1 / D.z );
	uint xsign = *(uint*)&D.x >> 31;
	uint ysign = *(uint*)&D.y >> 31;
	uint zsign = *(uint*)&D.z >> 31;
	Dsign = float3( (float)xsign, (float)ysign, (float)zsign ); // tnx Timon
}

float3 Ray::GetNormal(const Scene& scene) const
{
    if (axis == 3)
    {
        float3 hitPos = O + t * D;
        return normalize(hitPos - scene.spheres[sphereIndex].center);
    }

    // voxel normal (unchanged)
    const float3 sign = Dsign * 2.0f - 1.0f;
    return float3(
        axis == 0 ? sign.x : 0,
        axis == 1 ? sign.y : 0,
        axis == 2 ? sign.z : 0
    );
}

float3 Ray::GetAlbedo(const Scene& scene) const
{
    if (voxel < MAT_COUNT)
        return scene.materials[voxel].albedo;

    // DEBUG — print first time we decode a vox voxel
    static bool printed = false;
    if (!printed) {
        printf("[GetAlbedo] voxel=%u  albedo=%.2f %.2f %.2f\n",
            voxel,
            scene.materials[voxel].albedo.x,
            scene.materials[voxel].albedo.y,
            scene.materials[voxel].albedo.z);
        printed = true;
    }

    return scene.materials[voxel].albedo;
}