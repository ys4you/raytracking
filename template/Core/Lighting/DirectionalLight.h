#pragma once

#include "Core/ShadingPoint.h"

struct DirectionalLight
{
    float3 direction;
    float3 color;
    bool enabled = true;

    DirectionalLight() = default;

    DirectionalLight(const float3& dir, const float3& c)
        : direction(normalize(dir)), color(c) {
    }
};

inline float3 IlluminateDirectional(
    const DirectionalLight& light,
    const ShadingPoint& sp,
    Scene& scene)
{
    const float3 Ldir = -light.direction;

    const float voxelSize = 1.0f / 128.0f;
    const float EPS = voxelSize * 0.5f;

    Ray shadowRay(
        sp.position + sp.normal * EPS,
        Ldir
    );

    if (scene.IsOccluded(shadowRay))
        return float3(0);

    const float ndotl = max(0.0f, dot(sp.normal, Ldir));
    return light.color * ndotl;
}