#pragma once


struct PointLight
{
    float3 position;
    float3 color;
    bool enabled = true;
};

inline float3 IlluminatePoint(
    const PointLight& light,
    const ShadingPoint& sp,
    const Scene& scene)
{
    constexpr float EPS = 0.05f;

    float3 L = light.position - sp.position;

    float dist2 = dot(L, L);
    float dist = sqrt(dist2);

    const float3 Ldir = L / dist;

    Ray shadowRay(
        sp.position + sp.normal * EPS,
        Ldir,
        dist - EPS
    );

    if (scene.IsOccluded(shadowRay))
        return float3(0);

    const float ndotl = max(0.0f, dot(sp.normal, Ldir));

    float attenuation = 1.0f / dist2;

    return light.color * ndotl * attenuation;
}