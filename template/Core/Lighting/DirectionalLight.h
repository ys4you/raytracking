#pragma once
#include "Core/Lighting/Light.h"


class DirectionalLight :
    public Light
{
public:
    float3 direction;
    float3 color;

    DirectionalLight(const float3& dir, const float3& c);

    float3 Illuminate(const ShadingPoint& sp, Scene& scene) const override;
};

