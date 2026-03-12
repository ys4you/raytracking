#pragma once
#include "Spline.h"

struct SplineFollower
{
    CatmullRomSpline* spline = nullptr;

    float distance = 0.0f;
    float speed = 1.0f;
    bool loop = true;

    float3 position;
    float3 forward;

    void Update(float dt)
    {
        if (!spline || spline->totalArcLength <= 0.0f) return;

        distance += speed * dt;

        if (loop)
            distance = fmodf(distance, spline->totalArcLength);

        float t = spline->SampleByArcLength(distance);

        SplineSample s = spline->Sample(t);

        position = s.position;
        forward = s.tangent;
    }
};