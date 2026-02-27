#include "template.h"
#include "Sky.h"

Sky::Sky()
{
    sun.direction = normalize(float3(0, -1, 0.2f));
    sun.color = float3(1.0f, 0.95f, 0.8f);
    moon.direction = normalize(float3(0, 1, 0.2f));
    moon.color = float3(0.1f, 0.15f, 0.25f);
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

float3 Sky::GetSkyColor(const float3& dir) const
{
    float angle = timeOfDay * PI * 2.0f;
    float3 sunDir = normalize(float3(cos(angle), sin(angle), 0.2f));
    float sunHeight = sunDir.y; // -1 to 1

    // --- Define sky colour palettes per time of day ---
    // Night
    float3 zenithNight = float3(0.01f, 0.01f, 0.05f);
    float3 horizonNight = float3(0.02f, 0.02f, 0.06f);

    // Dawn / Dusk
    float3 zenithDawn = float3(0.15f, 0.20f, 0.45f);
    float3 horizonDawn = float3(0.80f, 0.35f, 0.15f);

    // Day
    float3 zenithDay = zenithColor;   // uses your UI-editable value
    float3 horizonDay = horizonColor;  // uses your UI-editable value

    // --- Blend between palettes based on sun height ---
    float nightBlend = smoothstep(-0.1f, 0.0f, sunHeight); // 0=night, 1=dawn starts
    float dawnBlend = smoothstep(0.0f, 0.2f, sunHeight); // 0=dawn,  1=full day
    float duskBlend = smoothstep(0.2f, 0.0f, sunHeight); // mirrors dawn for sunset

    float3 currentZenith, currentHorizon;

    if (sunHeight >= 0.0f)
    {
        // Sunrise: blend from dawn to day
        // Sunset:  blend from day back to dawn (duskBlend handles this)
        float t = smoothstep(0.0f, 0.25f, sunHeight);
        currentZenith = lerp(zenithDawn, zenithDay, t);
        currentHorizon = lerp(horizonDawn, horizonDay, t);
    }
    else
    {
        // Below horizon: blend from dawn colours to night
        float t = smoothstep(0.0f, -0.15f, sunHeight);
        currentZenith = lerp(zenithDawn, zenithNight, t);
        currentHorizon = lerp(horizonDawn, horizonNight, t);
    }

    // --- Apply gradient based on ray direction ---
    float t = clamp(dir.y * 0.5f + 0.5f, 0.0f, 1.0f);
    float3 sky = lerp(currentHorizon, currentZenith, sqrt(t));

    // --- Sun disc ---
    float cosAngle = dot(dir, sunDir);
    float disc = smoothstep(0.9995f, 0.9999f, cosAngle);
    sky += disc * sunNoonColor * sunIntensity * 20.0f;

    // --- Horizon glow around sun (orange band at sunrise/sunset) ---
    float horizonGlow = pow(max(0.0f, cosAngle), 6.0f)
        * max(0.0f, 1.0f - abs(sunHeight) * 4.0f); // only near horizon
    sky += horizonGlow * float3(0.8f, 0.3f, 0.05f) * sunIntensity;

    // --- Stars at night (simple noise approximation) ---
    if (sunHeight < 0.0f)
    {
        float starMask = max(0.0f, -sunHeight); // fade in as sun sets
        // Use direction components as a cheap hash for star positions
        float v = sin(dot(dir, float3(127.1f, 311.7f, 74.4f))) * 43758.5f;
        float star = v - floor(v);        star = smoothstep(0.997f, 1.0f, star); // only very bright "stars"
        sky += star * starMask * float3(0.9f, 0.9f, 1.0f);
    }

    return sky;
}