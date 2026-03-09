#include "template.h"

#include "Core/TextureTable.h"

/// <summary>
/// Constructs a ray used for ray tracing.
/// </summary>
/// <param name="origin">Starting point of the ray.</param>
/// <param name="direction">Direction the ray travels.</param>
/// <param name="rayLength">Maximum ray length.</param>
/// <param name="rgb">Voxel/material identifier.</param>
Ray::Ray(const float3 origin, const float3 direction, const float rayLength, const uint rgb)
	: O(origin), D(normalize(direction)), t(rayLength), voxel(rgb)
{
	// Calculate reciprocal ray direction.
	// Used to speed up ray/AABB and triangle intersection tests.
	// Note: division by zero could produce NaN values.
	rD = float3(1 / D.x, 1 / D.y, 1 / D.z);

	// Extract sign bits of each direction component
	uint xsign = *reinterpret_cast<uint*>(&D.x) >> 31;
	uint ysign = *reinterpret_cast<uint*>(&D.y) >> 31;
	uint zsign = *reinterpret_cast<uint*>(&D.z) >> 31;

	// Store the sign of the direction (0 or 1) as floats
	Dsign = float3(static_cast<float>(xsign), static_cast<float>(ysign), static_cast<float>(zsign)); // tnx Timon
}

/// <summary>
/// Computes the surface normal at the ray intersection.
/// </summary>
/// <param name="scene">Scene containing geometry.</param>
/// <returns>Normalized surface normal at the hit point.</returns>
float3 Ray::GetNormal(const Scene& scene) const
{
	// If the ray hit a sphere, compute the normal from the sphere center
	if (axis == 3)
	{
		float3 hitPos = O + t * D;
		return normalize(hitPos - scene.spheres[sphereIndex].center);
	}

	// Otherwise the hit was a voxel face.
	// Determine the face normal based on the hit axis.
	const float3 sign = Dsign * 2.0f - 1.0f;

	return float3(
		axis == 0 ? sign.x : 0,
		axis == 1 ? sign.y : 0,
		axis == 2 ? sign.z : 0
	);
}

/// <summary>
/// Retrieves the albedo (base color) of the material hit by the ray.
/// </summary>
/// <param name="scene">Scene containing materials.</param>
/// <returns>The albedo color of the intersected object.</returns>
float3 Ray::GetAlbedo(const Scene& scene) const
{
	// If a sphere was hit, return its material albedo
	if (sphereIndex >= 0)
		return scene.GetSphereMat(scene.spheres[sphereIndex].material).albedo;

	// Otherwise return the voxel material albedo
	return scene.GetMat(voxel).albedo;
}