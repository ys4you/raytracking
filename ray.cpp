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
    // Sphere hit
    if (sphereIndex >= 0)
    {
        float3 hitPos = O + t * D;
        float3 centre = float3(
            scene.sphereSOA.cx[sphereIndex],
            scene.sphereSOA.cy[sphereIndex],
            scene.sphereSOA.cz[sphereIndex]
        );
        return normalize(hitPos - centre);
    }

    // TLAS instance hit — Bikker's fractional-position method
    if (instanceIndex >= 0)
    {
        const VoxelInstance& inst = scene.voxelInstances[instanceIndex];

        // 1. Get hit point in local voxel space
        float3 hitWorld = O + t * D;
        float3 hitLocal = inst.worldToLocal.TransformPoint(hitWorld);

        // 2. Fractional position within the hit voxel
        float3 fG(hitLocal.x - floorf(hitLocal.x),
            hitLocal.y - floorf(hitLocal.y),
            hitLocal.z - floorf(hitLocal.z));
        float3 d = fminf(fG, 1.0f - fG);

        // 3. Closest face = smallest distance component
        float3 localD = inst.worldToLocal.TransformVector(D);
        float mind = min(min(d.x, d.y), d.z);
        float3 localN(0, 0, 0);
        if (mind == d.x)      localN.x = (localD.x > 0) ? -1.0f : 1.0f;
        else if (mind == d.y) localN.y = (localD.y > 0) ? -1.0f : 1.0f;
        else                  localN.z = (localD.z > 0) ? -1.0f : 1.0f;

        // 4. Transform local normal to world space
        float3 wn;
        wn.x = inst.worldToLocal[0] * localN.x + inst.worldToLocal[4] * localN.y + inst.worldToLocal[8] * localN.z;
        wn.y = inst.worldToLocal[1] * localN.x + inst.worldToLocal[5] * localN.y + inst.worldToLocal[9] * localN.z;
        wn.z = inst.worldToLocal[2] * localN.x + inst.worldToLocal[6] * localN.y + inst.worldToLocal[10] * localN.z;
        return normalize(wn);
    }

    // World-grid voxel hit
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

