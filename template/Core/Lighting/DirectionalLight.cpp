#include "template.h"
#include "DirectionalLight.h"

#include "Core/ShadingPoint.h"

DirectionalLight::DirectionalLight(const float3& dir, const float3& c)
    : direction(dir), color(c)
{
}

float3 DirectionalLight::Illuminate(const ShadingPoint& sp, Scene& scene) const
{
    const float3 Ldir = normalize(-direction);

    //voxel-based epsilon - Claude
    const float voxelSize = 1.0f / 128.0f;
    const float EPS = voxelSize * .5f;

    Ray shadowRay(
        sp.position + sp.normal * EPS,
        Ldir
    );

    if (scene.IsOccluded(shadowRay))
        return float3(0);

    const float ndotl = max(0.0f, dot(sp.normal, Ldir));
    return color * sp.albedo * ndotl;
}

