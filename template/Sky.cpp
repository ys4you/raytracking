#include "template.h"
#include "Sky.h"

//Claude helped with the fading in and out the HDR skybox

Sky::Sky()
{
    sun.direction = normalize(float3(0, -1, 0.2f));
    sun.color = float3(1.0f, 0.95f, 0.8f);
    moon.direction = normalize(float3(0, 1, 0.2f));
    moon.color = float3(0.1f, 0.15f, 0.25f);

    bool ok = hdrSky.Load("assets/sky.hdr");
    if (!ok)
        printf("HDR load failed: %s\n", stbi_failure_reason());
}

void Sky::Update(float dt)
{
    if (animate)
    {
        float dtSeconds = dt * 0.001f;
        timeOfDay += dtSeconds * cycleSpeed;
        if (timeOfDay > 1.0f) timeOfDay -= 1.0f;
    }
    UpdateLights();

    if (skyCacheDirty) 
        RebuildSkyCache();

}

// -----------------------------------------------------------
// UpdateLights — runs once per frame.
// All per-frame trig is computed here and cached so that
// GetSkyColor / GetProceduralSky never recompute it per-ray.
// -----------------------------------------------------------
void Sky::UpdateLights()
{
    float angle = timeOfDay * PI * 2.0f;

    // Cache values used every frame by the sky shading functions
    cachedSunDir = normalize(float3(cosf(angle), sinf(angle), 0.2f));
    cachedSunHeight = cachedSunDir.y;
    cachedHdrBlend = smoothstep(0.0f, 0.4f, sinf(angle));

    // Light directions — reuse cachedSunDir, no redundant computation
    sun.direction = -cachedSunDir;
    moon.direction = cachedSunDir;

    float sunHeight = max(0.0f, cachedSunHeight);
    float moonHeight = max(0.0f, -cachedSunHeight);

    sun.color = lerp(sunHorizonColor, sunNoonColor, sunHeight) * sunHeight * sunIntensity;
    moon.color = moonColor * moonHeight * moonIntensity;

    sun.enabled = sunHeight > 0.01f;
    moon.enabled = moonHeight > 0.01f;
}

// -----------------------------------------------------------
// SampleHDR — samples the HDR panorama for a given direction.
// acos / atan2 per ray is unavoidable here; the big savings are
// in GetProceduralSky which no longer calls cos/sin at all.
// -----------------------------------------------------------
float3 Sky::SampleHDR(const float3& dir) const
{
    if (!hdrSky.data)
        return float3(1, 0, 1); // debug magenta — HDR not loaded

    float3 d = normalize(dir);

    float theta = acosf(clamp(d.y, -1.0f, 1.0f));
    float phi = atan2f(d.z, d.x);

    float u = (phi + PI) / (2.0f * PI);
    float v = 1.0f - (theta / PI);

    return hdrSky.Sample(u, v);
}

// -----------------------------------------------------------
// GetProceduralSky — uses cached sun direction and height;
// no cos/sin calls here any more.
// -----------------------------------------------------------
float3 Sky::GetProceduralSky(const float3& dir) const
{
    float3 d = normalize(dir);

    // Use per-frame cached values instead of recomputing per ray
    const float3& sunDir = cachedSunDir;
    const float   sunHeight = cachedSunHeight; // -1 .. 1

    // --- Sky colour palettes ---
    // Night
    const float3 zenithNight = float3(0.01f, 0.01f, 0.05f);
    const float3 horizonNight = float3(0.02f, 0.02f, 0.06f);

    // Dawn / Dusk
    const float3 zenithDawn = float3(0.15f, 0.20f, 0.45f);
    const float3 horizonDawn = float3(0.80f, 0.35f, 0.15f);

    // Day (UI-controlled)
    const float3 zenithDay = zenithColor;
    const float3 horizonDay = horizonColor;

    float3 currentZenith, currentHorizon;

    if (sunHeight >= 0.0f)
    {
        // Dawn → Day
        float t = smoothstep(0.0f, 0.25f, sunHeight);
        currentZenith = lerp(zenithDawn, zenithDay, t);
        currentHorizon = lerp(horizonDawn, horizonDay, t);
    }
    else
    {
        // Dusk → Night
        float t = smoothstep(0.0f, -0.15f, sunHeight);
        currentZenith = lerp(zenithDawn, zenithNight, t);
        currentHorizon = lerp(horizonDawn, horizonNight, t);
    }

    // --- Vertical sky gradient ---
    float t = clamp(d.y * 0.5f + 0.5f, 0.0f, 1.0f);
    float3 sky = lerp(currentHorizon, currentZenith, sqrtf(t));

    // --- Sun disc ---
    float cosAngle = dot(d, sunDir);
    float disc = smoothstep(0.9995f, 0.9999f, cosAngle);
    sky += disc * sunNoonColor * sunIntensity * 20.0f;

    // --- Horizon glow (sunrise / sunset band) ---
    float horizonGlow =
        powf(max(0.0f, cosAngle), 6.0f) *
        max(0.0f, 1.0f - fabsf(sunHeight) * 4.0f);
    sky += horizonGlow * float3(0.8f, 0.3f, 0.05f) * sunIntensity;

    // --- Stars (only visible at night) ---
    if (sunHeight < 0.0f)
    {
        float starMask = max(0.0f, -sunHeight);

        // Cheap hash-based star field — sin is called once per sky ray,
        // but only during night so hdrBlend = 0 and this path dominates
        float v = sinf(dot(d, float3(127.1f, 311.7f, 74.4f))) * 43758.5453f;
        float star = v - floorf(v);
        star = smoothstep(0.997f, 1.0f, star);

        sky += star * starMask * float3(0.9f, 0.9f, 1.0f);
    }

    return sky;
}

void Sky::RebuildSkyCache()
{
    skyCache.resize(SKY_W * SKY_H);
    for (int y = 0; y < SKY_H; y++)
        for (int x = 0; x < SKY_W; x++)
        {
            float u = (x + 0.5f) / SKY_W;
            float v = (y + 0.5f) / SKY_H;
            float phi = u * 2.0f * PI - PI;
            float theta = v * PI;
            float3 dir = {
                sinf(theta) * cosf(phi),
                cosf(theta),
                sinf(theta) * sinf(phi)
            };
            skyCache[y * SKY_W + x] = GetSkyColorUncached(dir);
        }
    skyCacheDirty = false;
}

// -----------------------------------------------------------
// GetSkyColor — blends procedural and HDR sky.
// Uses cachedHdrBlend computed in UpdateLights; no per-ray
// sin/cos calls remaining in this function.
// -----------------------------------------------------------
float3 Sky::GetSkyColor(const float3& dir) const
{
    float phi = atan2f(dir.z, dir.x);
    float theta = acosf(clamp(dir.y, -1.0f, 1.0f));
    float u = (phi + PI) / (2.0f * PI);
    float v = theta / PI;
    int x = (int)(u * SKY_W) % SKY_W;
    int y = (int)(v * SKY_H) % SKY_H;
    return skyCache[y * SKY_W + x];
}


float3 Sky::GetSkyColorUncached(const float3& dir) const
{
    float3 procedural = GetProceduralSky(dir);

    if (!hdrSky.data)
        return procedural; // HDR not loaded — use procedural only

    float3 hdr = SampleHDR(dir);

    // cachedHdrBlend was computed once in UpdateLights
    return lerp(procedural, hdr, cachedHdrBlend);
}