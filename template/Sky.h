#pragma once
#include "HDRCubemap.h"
#include "Core/Lighting/DirectionalLight.h"

class Sky
{
public:
    Sky();
    void Update(float deltaTime);
    float3 GetSkyColor(const float3& dir) const;
    float3 GetSkyColorUncached(const float3& dir) const;

    // Lights — registered directly into renderer's lights vector
    DirectionalLight sun;
    DirectionalLight moon;

    // UI-exposed parameters
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

    float3 cachedSunDir;      // current sun direction
    float  cachedSunHeight;   // sunDir.y
    float  cachedHdrBlend;    // smoothstep blend between procedural and HDR

private:
    void UpdateLights();
    float3 SampleHDR(const float3& dir) const;
    float3 GetProceduralSky(const float3& dir) const;

    HDRCubemap hdrSky;

    // Sky direction cache
    static const int SKY_W = 512;
    static const int SKY_H = 256;
    std::vector<float3> skyCache;
    bool skyCacheDirty = true;

    void RebuildSkyCache();

};