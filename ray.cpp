#include "template.h"
#include "Core/TextureTable.h"

Ray::Ray(const float3 origin, const float3 direction, const float rayLength, const uint rgb)
    : O(origin), D(normalize(direction)), t(rayLength), voxel(rgb)
{
    rD = float3(1 / D.x, 1 / D.y, 1 / D.z);

    uint xsign = *reinterpret_cast<uint*>(&D.x) >> 31;
    uint ysign = *reinterpret_cast<uint*>(&D.y) >> 31;
    uint zsign = *reinterpret_cast<uint*>(&D.z) >> 31;

    Dsign = float3(static_cast<float>(xsign),
        static_cast<float>(ysign),
        static_cast<float>(zsign));
}

float3 Ray::GetNormal(const Scene& scene) const
{
    if (axis == 3)
    {
        // Sphere hit — reconstruct normal from SOA centre.
        float3 hitPos = O + t * D;
        float3 centre = float3(
            scene.sphereSOA.cx[sphereIndex],
            scene.sphereSOA.cy[sphereIndex],
            scene.sphereSOA.cz[sphereIndex]
        );
        return normalize(hitPos - centre);
    }

    // Voxel face normal.
    // Guard against corrupt axis value — should never be outside 0-2 for a voxel hit.
    if (axis < 0 || axis > 2) 
        return float3(0, 1, 0);

    const float3 sign = Dsign * 2.0f - 1.0f;
    float3 n(0, 0, 0);
    (&n.x)[axis] = (&sign.x)[axis];
    return n;
}

float3 Ray::GetAlbedo(const Scene& scene) const
{
    if (sphereIndex >= 0)
    {
        const uint matID = (uint)scene.sphereSOA.material[sphereIndex];
        return scene.GetSphereMat(matID).albedo;
    }
    return scene.GetMat(voxel).albedo;
}