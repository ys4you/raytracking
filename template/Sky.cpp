#include "template.h"
#include "Sky.h"

Sky::Sky()
{
    sun.direction = normalize(float3(0, -1, 0.2f));
    sun.color = float3(1.0f, 0.95f, 0.8f);
    moon.direction = normalize(float3(0, 1, 0.2f));
    moon.color = float3(0.1f, 0.15f, 0.25f);
    bool ok = hdrSky.Load("assets/sky.hdr");
    if (!ok)
    {
        printf("HDR load failed: %s\n", stbi_failure_reason());
    }
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
}

void Sky::UpdateLights()
{
    float angle = timeOfDay * PI * 2.0f;
    float3 sunDir = normalize(float3(cos(angle), sin(angle), 0.2f));

    sun.direction = -sunDir;
    moon.direction = sunDir;

    float sunHeight = max(0.0f, sunDir.y);
    float moonHeight = max(0.0f, -sunDir.y);

    sun.color = lerp(sunHorizonColor, sunNoonColor, sunHeight) * sunHeight * sunIntensity;
    moon.color = moonColor * moonHeight * moonIntensity;

    sun.enabled = sunHeight > 0.01f;
    moon.enabled = moonHeight > 0.01f;
}

float3 Sky::SampleHDR(const float3& dir) const
{
    if (!hdrSky.data) 
        return float3(1, 0, 1); // debug magenta

    float3 d = normalize(dir);

    float theta = acos(clamp(d.y, -1.0f, 1.0f));
    float phi = atan2(d.z, d.x);

    float u = (phi + PI) / (2.0f * PI);
    float v = 1.0f - (theta / PI); 

    return hdrSky.Sample(u, v);
}

float3 Sky::GetProceduralSky(const float3& dir) const
{
    float3 d = normalize(dir);

    float angle = timeOfDay * PI * 2.0f;
    float3 sunDir = normalize(float3(cos(angle), sin(angle), 0.2f));
    float sunHeight = sunDir.y; // -1 .. 1

    // --- Sky colour palettes ---
    // Night
    float3 zenithNight = float3(0.01f, 0.01f, 0.05f);
    float3 horizonNight = float3(0.02f, 0.02f, 0.06f);

    // Dawn / Dusk
    float3 zenithDawn = float3(0.15f, 0.20f, 0.45f);
    float3 horizonDawn = float3(0.80f, 0.35f, 0.15f);

    // Day (UI-controlled)
    float3 zenithDay = zenithColor;
    float3 horizonDay = horizonColor;

    float3 currentZenith;
    float3 currentHorizon;

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
    float3 sky = lerp(currentHorizon, currentZenith, sqrt(t));

    // --- Sun disc ---
    float cosAngle = dot(d, sunDir);
    float disc = smoothstep(0.9995f, 0.9999f, cosAngle);
    sky += disc * sunNoonColor * sunIntensity * 20.0f;

    // --- Horizon glow (sunrise / sunset band) ---
    float horizonGlow =
        pow(max(0.0f, cosAngle), 6.0f) *
        max(0.0f, 1.0f - abs(sunHeight) * 4.0f);

    sky += horizonGlow * float3(0.8f, 0.3f, 0.05f) * sunIntensity;

    // --- Stars ---
    if (sunHeight < 0.0f)
    {
        float starMask = max(0.0f, -sunHeight);

        float v = sin(dot(d, float3(127.1f, 311.7f, 74.4f))) * 43758.5453f;
        float star = v - floor(v);
        star = smoothstep(0.997f, 1.0f, star);

        sky += star * starMask * float3(0.9f, 0.9f, 1.0f);
    }

    return sky;
}

float3 Sky::GetSkyColor(const float3& dir) const
{
    float3 procedural = GetProceduralSky(dir);

    if (!hdrSky.data)
        return procedural;

    float angle = timeOfDay * 2.0f * PI;
    float sunHeight = sin(angle);

    float hdrBlend = smoothstep(0.0f, 0.4f, sunHeight);

    float3 hdr = SampleHDR(dir);

    return lerp(procedural, hdr, hdrBlend);
}