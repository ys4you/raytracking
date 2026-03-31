#pragma once
#include "Core/Material.h"

namespace Tmpl8 { class Scene; }


namespace Tmpl8 {

class Ray
{
public:
	/// <summary>Initializes a ray from origin, direction, and optional length.</summary>
	Ray( const float3 origin, const float3 direction, const float rayLength = 1e34f, const uint rgb = 0 );
	/// <summary>Computes the shading normal at the current hit point.</summary>
	float3 GetNormal(const Scene& scene) const;
	/// <summary>Returns the world-space intersection point.</summary>
	float3 IntersectionPoint() const { return O + t * D; }
	/// <summary>Returns albedo for the material hit by this ray.</summary>
	float3 GetAlbedo(const Scene& scene) const;
	/// <summary>Returns reflectivity for the hit material.</summary>
	float GetReflectivity( const float3& I ) const;
	/// <summary>Returns refractivity for the hit material.</summary>
	float GetRefractivity( const float3& I ) const;
	/// <summary>Returns absorption coefficients for the hit material.</summary>
	float3 GetAbsorption( const float3& I ) const;
	float3 O;
	float3 rD;
	float3 D;
	float t;
	float3 Dsign;
	uint voxel;
	uint axis = 0;
	bool inside = false;
	int materialIndex = -1;
	int sphereIndex = -1;

	int instanceIndex = -1;

private:
	__inline static float3 min3( const float3& a, const float3& b )
	{
		return float3( min( a.x, b.x ), min( a.y, b.y ), min( a.z, b.z ) );
	}
};

};
