#include "template.h"
#include "Core/TextureTable.h"

// Ray constructor
Ray::Ray(const float3 origin, const float3 direction, const float rayLength, const uint rgb)
    : O(origin), D(normalize(direction)), t(rayLength), voxel(rgb)
{
    // Store normalized ray direction and maximum ray length.
    // 'voxel' stores the voxel material or color index hit by the ray.

    // Precompute reciprocal direction (1 / direction).
    // This is used to speed up ray-AABB and ray-triangle intersection tests
    // by replacing divisions with multiplications.
    // NOTE: If D.x/y/z == 0 this may create INF/NaN values.
    rD = float3(1 / D.x, 1 / D.y, 1 / D.z);

    // Extract sign bits of each direction component.
    // This allows fast branching in intersection algorithms.
    const uint xsign = *reinterpret_cast<uint*>(&D.x) >> 31;
    const uint ysign = *reinterpret_cast<uint*>(&D.y) >> 31;
    const uint zsign = *reinterpret_cast<uint*>(&D.z) >> 31;

    // Convert sign bits to float (0 or 1)
    // Used later to determine which face of a voxel was hit.
    Dsign = float3(static_cast<float>(xsign), static_cast<float>(ysign), static_cast<float>(zsign)); // trick from Timon
}

float3 Ray::GetNormal(const Scene& scene) const
{
    // If axis == 3, the ray hit a sphere instead of a voxel
    if (axis == 3)
    {
        // Compute the hit position along the ray
        float3 hitPos = O + t * D;

        // Sphere normal = direction from sphere center to hit point
        return normalize(hitPos - scene.spheres[sphereIndex].center);
    }

    // Otherwise the ray hit a voxel face

    // Convert stored sign bits (0/1) to -1/+1
    const float3 sign = Dsign * 2.0f - 1.0f;

    // Determine which axis was hit and return the corresponding normal.
    // axis:
    // 0 = X face
    // 1 = Y face
    // 2 = Z face
    return float3(
        axis == 0 ? sign.x : 0,
        axis == 1 ? sign.y : 0,
        axis == 2 ? sign.z : 0
    );
}

float3 Ray::GetAlbedo(const Scene& scene) const
{
    // If we hit a sphere, return the sphere material color
    if (sphereIndex >= 0)
        return scene.GetSphereMat(scene.spheres[sphereIndex].material).albedo;

    // Otherwise return the voxel material color
    return scene.GetMat(voxel).albedo;
}