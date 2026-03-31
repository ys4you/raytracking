#pragma once
#include "HDRCubemap.h"
#include "Core/Lighting/DirectionalLight.h"

struct alignas(16) SkyPixel
{
    float r, g, b, _pad;
};

class Sky
{
public:

    Sky();
    void Update(float deltaTime);

    float3 GetSkyColor(const float3& dir) const;

    float3 GetSkyColorUncached(const float3& dir) const;

    DirectionalLight sun;
    DirectionalLight moon;

    float  timeOfDay = 0.25f;
    float  cycleSpeed = 0.005f;
    bool   animate = true;

    float3 sunNoonColor = float3(1.0f, 0.95f, 0.8f);
    float3 sunHorizonColor = float3(1.0f, 0.4f, 0.1f);
    float  sunIntensity = 3.0f;

    float3 moonColor = float3(0.4f, 0.5f, 0.7f);
    float  moonIntensity = 0.15f;

    float3 zenithColor = float3(0.2f, 0.4f, 0.8f);
    float3 horizonColor = float3(0.8f, 0.9f, 1.0f);

    void SetSunIntensity(float v) { sunIntensity = v; skyCacheDirty = true; }
    void SetZenithColor(float3 v) { zenithColor = v; skyCacheDirty = true; }
    void SetHorizonColor(float3 v) { horizonColor = v; skyCacheDirty = true; }

    float3 cachedSunDir;
    float  cachedSunHeight;
    float  cachedHdrBlend;

    bool  skyCacheDirty = true;


private:

    void   UpdateLights();
    float3 SampleHDR(const float3& dir) const;
    float3 GetProceduralSky(const float3& dir) const;
    void   RebuildSkyCache();

    static float FastAtan2(float y, float x);
    static float FastAcos(float x);

    HDRCubemap hdrSky;

    static constexpr int SKY_W = 2048; // Must stay a power of two for MASK_W wrapping.
    static constexpr int SKY_H = 1024; // Must stay a power of two for MASK_H clamping path.

    std::vector<SkyPixel> skyCache;

    float lastBuiltTime = -999.0f;
    static constexpr float TIME_DIRTY_THRESHOLD = 0.005f;
};


inline float Sky::FastAtan2(float y, float x)
{
    constexpr float PI_2 = 1.57079632679490f;

    auto atan01 = [](float t) -> float
        {
            return t * (0.78539816f + 0.27197f * (1.0f - t));
        };

    float ay = fabsf(y);
    float ax = fabsf(x);

    bool swap = ay > ax;
    float t = swap ? (ax / (ay + 1e-10f)) : (ay / (ax + 1e-10f));

    float angle = atan01(t);

    if (swap)   angle = PI_2 - angle;
    if (x < 0)  angle = PI - angle;
    if (y < 0)  angle = -angle;

    return angle;
}

inline float Sky::FastAcos(float x)
{
    float ax = fabsf(x);
    float result = ((-0.0187293f * ax + 0.0742610f) * ax - 0.2121144f) * ax
        + 1.5707288f;
    result *= sqrtf(1.0f - ax);
    return (x >= 0.0f) ? result : PI - result;
}
