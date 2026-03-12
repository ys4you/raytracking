#include "template.h"
#include "PointLight.h"

#include "Core/ShadingPoint.h"


PointLight::PointLight(const float3& p, const float3& c)
    : position(p), color(c)
{
}

float3 PointLight::Illuminate(const ShadingPoint& sp, Scene& scene) const
{
    float3 L = position - sp.position;
    float distance = length(L);
    const float3 Ldir = normalize(L);

    // Shadow ray
    Ray shadowRay(sp.position, Ldir, distance);
    if (scene.IsOccluded(shadowRay))
        return float3(0);

    const float ndotl = max(0.0f, dot(sp.normal, Ldir));
    float attenuation = 1.0f / (distance * distance);

    return color * sp.albedo * ndotl * attenuation;
}
