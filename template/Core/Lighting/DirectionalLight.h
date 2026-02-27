#pragma once
#include "Core/Lighting/Light.h"


class DirectionalLight :
    public Light
{
public:
    float3 direction;
    float3 color;

    DirectionalLight() : direction(normalize(float3(0, -1, 0))), color(float3(1, 1, 1)) { }

    DirectionalLight(const float3& dir, const float3& c);

    float3 Illuminate(const ShadingPoint& sp, Scene& scene) const override;
};

