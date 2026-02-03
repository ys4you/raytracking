#include "template.h"
#include "DirectionalLight.h"

#include "Core/ShadingPoint.h"

DirectionalLight::DirectionalLight(const float3& dir, const float3& c)
    : dir(dir), color(c)
{
}

float3 DirectionalLight::Illuminate(const ShadingPoint& sp, Scene& scene) const
{
    const float3 Ldir = -dir; // light coming FROM direction

    // Shadow ray (infinite distance)
    const float EPS = 0.001f;
    Ray shadowRay(
        sp.position + sp.normal * EPS,
        Ldir,
        1e30f
    );

    if (scene.IsOccluded(shadowRay))
        return float3(0);  // NOLINT(modernize-return-braced-init-list)

    const float ndotl = max(0.0f, dot(sp.normal, Ldir));
    return color * sp.albedo * ndotl;
}

