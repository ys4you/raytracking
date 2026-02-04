#pragma once
#include "Light.h"
class SpotLight :
    public Light
{
public:
    float3 position;
    float3 direction;
    float3 color;

    float range;

    float spotAngleDeg = 10.f;   // full cone
    float edgeRoughness = 0;  // 0 = hard, 1 = fully soft


    SpotLight(float3 pos, float3 dir, float3 col, float falloff);

    float3 Illuminate(const ShadingPoint& sp, Scene& scene) const override;

};


