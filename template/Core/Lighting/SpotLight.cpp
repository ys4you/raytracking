#include "template.h"
#include "SpotLight.h"

#include "Core/ShadingPoint.h"

SpotLight::SpotLight(float3 pos, float3 dir, float3 col, float falloff)
    : position(pos), direction(normalize(dir)), color(col),
    range(falloff)
{
}
constexpr float DEG2RAD = 3.14159265359f / 180.0f;
float3 SpotLight::Illuminate(const ShadingPoint& sp, Scene& scene) const
{
    const float EPS = 0.05f;

    float3 toPoint = sp.position - position;
    float distance = length(toPoint);

    if (distance > range)
        return float3(0);

    float3 L = normalize(toPoint);

    // Shadow test
    Ray shadowRay(sp.position + sp.normal * EPS, -L);
    if (scene.IsOccluded(shadowRay))
        return float3(0);

    // Distance attenuation
    float attenuation = 1.0f - distance / range;

    // Spotlight cone with proportional roughness
    float outerAngle = spotAngleDeg;
    float innerAngle = spotAngleDeg * (1.0f - edgeRoughness);

    float cosOuter = cosf(outerAngle * 0.5f * DEG2RAD);
    float cosInner = cosf(innerAngle * 0.5f * DEG2RAD);

    float spotFactor = dot(direction, L);

    if (spotFactor < cosOuter)
        return float3(0);

    float spotIntensity =
        clamp((spotFactor - cosOuter) / (cosInner - cosOuter), 0.0f, 1.0f);

    const float ndotl = max(0.0f, dot(sp.normal, -L));  // Note: -L because L points from light to surface
    return color * sp.albedo * ndotl * attenuation * spotIntensity;
}


