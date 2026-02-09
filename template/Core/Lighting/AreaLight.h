#pragma once
#include "Light.h"
class AreaLight : public Light
{
public:

    float3 corner, edge1, edge2, normal;

    float3 color;
    float intensity = 1;
    int uSteps, vSteps;

    AreaLight(const float3& c, const float3& e1, const float3& e2, const float3& col, int u = 4, int v = 4);
    float3 Illuminate(const ShadingPoint& sp, Scene& scene) const override;

    inline float3 SamplePoint(int u, int v) const
    {
        //Random points - old solution
        //float du = (u + RandomFloat()) / uSteps;
        //float dv = (v + RandomFloat()) / vSteps;

        //Riemann Sum - New Solution 
        float du = (u + 0.5f) / uSteps;
        float dv = (v + 0.5f) / vSteps;
        return corner + edge1 * du + edge2 * dv;
    }

};