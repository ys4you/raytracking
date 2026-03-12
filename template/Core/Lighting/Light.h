#pragma once
struct ShadingPoint;

class Light
{
public:
    bool enabled = true;
    virtual float3 Illuminate(const ShadingPoint&, Scene&) const = 0; //avoid virtual when posible if you dont have a hierchy 
    virtual ~Light() = default;
};
