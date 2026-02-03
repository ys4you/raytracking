#pragma once
struct ShadingPoint;

class Light
{
public:
    virtual float3 Illuminate(const ShadingPoint&, Scene&) const = 0;
    virtual ~Light() = default;
};
