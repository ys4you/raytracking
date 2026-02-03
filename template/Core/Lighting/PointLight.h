#pragma once
#include "Light.h"

class PointLight : public Light
{
public:
    float3 position;
    float3 color;

    PointLight(const float3& p, const float3& c);

    float3 Illuminate(const ShadingPoint& sp, Scene& scene) const override;
};


