#pragma once

struct SpotLight
{
    float3 position;
    float3 direction;
    float3 color;

    float range = 10.0f;
    float spotAngleDeg = 30.0f;
    float edgeRoughness = 0.2f;

    bool enabled = true;

    SpotLight() = default;

    SpotLight(float3 pos, float3 dir, float3 col, float r)
        : position(pos),
        direction(normalize(dir)),
        color(col),
        range(r)
    {
    }
};

constexpr float DEG2RAD = 3.14159265359f / 180.0f;

inline float3 IlluminateSpot(
    const SpotLight& light,
    const ShadingPoint& sp,
    const Scene& scene)
{
    float3 toLight = light.position - sp.position;
    float dist2 = dot(toLight, toLight);
    float distance = sqrtf(dist2);
    if (distance > light.range)
        return float3(0);

    float3 L = toLight / distance;

    // Shadow ray — limited to light distance
    constexpr float EPS = 0.001f;
    Ray shadowRay(sp.position + sp.normal * EPS, L, distance - EPS);
    if (scene.IsOccluded(shadowRay))
        return float3(0);

    float attenuation = 1.0f - distance / light.range;

    float outerAngle = light.spotAngleDeg;
    float innerAngle = outerAngle * (1.0f - light.edgeRoughness);
    float cosOuter = cosf(outerAngle * 0.5f * DEG2RAD);
    float cosInner = cosf(innerAngle * 0.5f * DEG2RAD);

    // Spot cone — compare light direction with direction TO the point
    float spotFactor = dot(light.direction, -L);
    if (spotFactor < cosOuter)
        return float3(0);

    float spotIntensity =
        clamp((spotFactor - cosOuter) / (cosInner - cosOuter), 0.0f, 1.0f);

    float ndotl = max(0.0f, dot(sp.normal, L));

    return light.color * ndotl * attenuation * spotIntensity;
}