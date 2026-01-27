#include "template.h"

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

float3 Ray::GetNormal() const
{
	// return the voxel normal at the nearest intersection
	const float3 sign = Dsign * 2.0f - 1.0f;
	return float3( axis == 0 ? sign.x : 0, axis == 1 ? sign.y : 0, axis == 2 ? sign.z : 0 );
}

float3 Ray::GetAlbedo() const
{
	// return the (floating point) albedo at the nearest intersection
	return RGB8_to_RGBF32( voxel );
}