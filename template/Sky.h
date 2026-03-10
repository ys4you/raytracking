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

    // Hot path — called per ray that misses geometry.
    float3 GetSkyColor(const float3& dir) const;

    // Used only during RebuildSkyCache; not called per ray.
    float3 GetSkyColorUncached(const float3& dir) const;

    // ------------------------------------------------------------------
    //  Lights registered directly into the renderer's light list
    // ------------------------------------------------------------------
    DirectionalLight sun;
    DirectionalLight moon;

    // ------------------------------------------------------------------
    //  UI-exposed parameters
    //  Setter wrappers mark the cache dirty so callers don't have to.
    // ------------------------------------------------------------------
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

    // Call these from ImGui instead of assigning directly so the cache
    // is automatically invalidated when a parameter changes.
    void SetSunIntensity(float v) { sunIntensity = v; skyCacheDirty = true; }
    void SetZenithColor(float3 v) { zenithColor = v; skyCacheDirty = true; }
    void SetHorizonColor(float3 v) { horizonColor = v; skyCacheDirty = true; }

    // Cached per-frame values (read by renderer for lighting)
    float3 cachedSunDir;
    float  cachedSunHeight;
    float  cachedHdrBlend;

private:

    // ------------------------------------------------------------------
    //  Internal helpers
    // ------------------------------------------------------------------
    void   UpdateLights();
    float3 SampleHDR(const float3& dir) const;
    float3 GetProceduralSky(const float3& dir) const;
    void   RebuildSkyCache();

    // ------------------------------------------------------------------
    //  Fast math helpers (defined inline below the class)
    // ------------------------------------------------------------------
    static float FastAtan2(float y, float x);
    static float FastAcos(float x);

    // ------------------------------------------------------------------
    //  HDR source image
    // ------------------------------------------------------------------
    HDRCubemap hdrSky;

    static constexpr int SKY_W = 512;   // must stay power of two
    static constexpr int SKY_H = 256;    // must stay power of two

    std::vector<SkyPixel> skyCache;
    bool  skyCacheDirty = true;

    // Dirty-flag threshold: only rebuild when timeOfDay has moved by more
    // than this amount.  Keeps rebuilds rare during fast animation.
    float lastBuiltTime = -999.0f;
    static constexpr float TIME_DIRTY_THRESHOLD = 0.005f;
};


inline float Sky::FastAtan2(float y, float x)
{
    constexpr float PI_2 = 1.57079632679490f;

    // Fast atan on [0,1]: error < 0.005 rad
    // Formula: atan(t) ≈ t * (PI/4 + 0.273 * (1 - t))   for t in [0,1]
    auto atan01 = [](float t) -> float
        {
            return t * (0.78539816f + 0.27197f * (1.0f - t));
        };

    float ay = fabsf(y);
    float ax = fabsf(x);

    // Reduce to first octant (t always in [0,1])
    bool swap = ay > ax;
    float t = swap ? (ax / (ay + 1e-10f)) : (ay / (ax + 1e-10f));

    float angle = atan01(t);

    // Undo octant reduction
    if (swap)   angle = PI_2 - angle;  // reflected over PI/4
    if (x < 0)  angle = PI - angle;  // second/third quadrant
    if (y < 0)  angle = -angle;        // lower half-plane

    return angle;
}

// ===========================================================================
//  FastAcos
//
//  Drobot approximation (used in Killzone: Shadow Fall, 2013 GDC).
//  Max error ≈ 0.004°.  Uses sqrtf which compiles to a single SQRTSS
//  instruction on x86 — not expensive.
// ===========================================================================
inline float Sky::FastAcos(float x)
{
    float ax = fabsf(x);
    float result = ((-0.0187293f * ax + 0.0742610f) * ax - 0.2121144f) * ax
        + 1.5707288f;
    result *= sqrtf(1.0f - ax);
    return (x >= 0.0f) ? result : PI - result;
}