#include "template.h"
#include "Sky.h"

// Claude helped with HDR skybox blending and the optimisation pass.

// ===========================================================================
//  Constructor
// ===========================================================================
Sky::Sky()
{
    sun.direction = normalize(float3(0, -1, 0.2f));
    sun.color = float3(1.0f, 0.95f, 0.8f);
    moon.direction = normalize(float3(0, 1, 0.2f));
    moon.color = float3(0.1f, 0.15f, 0.25f);

    // Pre-allocate the cache so the first call to RebuildSkyCache never
    // triggers a heap allocation inside the render loop.
    skyCache.resize(SKY_W * SKY_H);

    bool ok = hdrSky.Load("assets/order_sky.hdr");
    if (!ok)
        printf("HDR load failed: %s\n", stbi_failure_reason());
}

// ===========================================================================
//  Update  — called once per frame before rendering begins
// ===========================================================================
void Sky::Update(float dt)
{
    if (animate)
    {
        float dtSeconds = dt * 0.001f;
        timeOfDay += dtSeconds * cycleSpeed;
        if (timeOfDay > 1.0f) timeOfDay -= 1.0f;

        // Mark dirty only when timeOfDay has moved far enough to see a
        // difference.  At the default cycleSpeed this fires roughly every
        // 10-20 frames instead of every frame — saves most rebuild cost.
        if (fabsf(timeOfDay - lastBuiltTime) > TIME_DIRTY_THRESHOLD)
            skyCacheDirty = true;
    }

    UpdateLights();

    // Rebuild happens at most once per dirty transition, never mid-frame.
    if (skyCacheDirty)
        RebuildSkyCache();
}

// ===========================================================================
//  UpdateLights  — runs once per frame, caches per-frame trig
// ===========================================================================
void Sky::UpdateLights()
{
    float angle = timeOfDay * PI * 2.0f;

    cachedSunDir = normalize(float3(cosf(angle), sinf(angle), 0.2f));
    cachedSunHeight = cachedSunDir.y;
    cachedHdrBlend = smoothstep(0.0f, 0.4f, sinf(angle));

    sun.direction = -cachedSunDir;
    moon.direction = cachedSunDir;

    float sunHeight = max(0.0f, cachedSunHeight);
    float moonHeight = max(0.0f, -cachedSunHeight);

    sun.color = lerp(sunHorizonColor, sunNoonColor, sunHeight) * sunHeight * sunIntensity;
    moon.color = moonColor * moonHeight * moonIntensity;

    sun.enabled = sunHeight > 0.01f;
    moon.enabled = moonHeight > 0.01f;
}

// ===========================================================================
//  SampleHDR  — bilinear lookup into the loaded HDR panorama
//
//  Uses FastAcos / FastAtan2 instead of the standard library versions.
//  The HDRCubemap now stores 4 floats per pixel (RGBA), so its own
//  bilinear sampler uses aligned 16-byte loads.
// ===========================================================================
float3 Sky::SampleHDR(const float3& dir) const
{
    if (!hdrSky.data)
        return float3(1, 0, 1);   // debug magenta — HDR not loaded

    float3 d = normalize(dir);

    // FastAcos replaces acosf  (~4 cycles vs ~25)
    // FastAtan2 replaces atan2f (~5 cycles vs ~25)
    float theta = FastAcos(clamp(d.y, -1.0f, 1.0f));
    float phi = FastAtan2(d.z, d.x);

    float u = (phi + PI) / (2.0f * PI);
    float v = 1.0f - (theta / PI);

    // HDRCubemap::Sample is now bilinear — four texel reads + three lerps.
    return hdrSky.Sample(u, v);
}

// ===========================================================================
//  GetProceduralSky  — unchanged from original except that it already uses
//  the per-frame cached sun direction / height, so there is nothing further
//  to optimise here without changing the look of the sky.
// ===========================================================================
float3 Sky::GetProceduralSky(const float3& dir) const
{
    float3 d = normalize(dir);

    const float3& sunDir = cachedSunDir;
    const float   sunHeight = cachedSunHeight;

    // --- Colour palettes ---
    const float3 zenithNight = float3(0.01f, 0.01f, 0.05f);
    const float3 horizonNight = float3(0.02f, 0.02f, 0.06f);
    const float3 zenithDawn = float3(0.15f, 0.20f, 0.45f);
    const float3 horizonDawn = float3(0.80f, 0.35f, 0.15f);
    const float3 zenithDay = zenithColor;
    const float3 horizonDay = horizonColor;

    float3 currentZenith, currentHorizon;

    if (sunHeight >= 0.0f)
    {
        float t = smoothstep(0.0f, 0.25f, sunHeight);
        currentZenith = lerp(zenithDawn, zenithDay, t);
        currentHorizon = lerp(horizonDawn, horizonDay, t);
    }
    else
    {
        float t = smoothstep(0.0f, -0.15f, sunHeight);
        currentZenith = lerp(zenithDawn, zenithNight, t);
        currentHorizon = lerp(horizonDawn, horizonNight, t);
    }

    float t = clamp(d.y * 0.5f + 0.5f, 0.0f, 1.0f);
    float3 sky = lerp(currentHorizon, currentZenith, sqrtf(t));

    float cosAngle = dot(d, sunDir);
    float disc = smoothstep(0.9995f, 0.9999f, cosAngle);
    sky += disc * sunNoonColor * sunIntensity * 20.0f;

    float horizonGlow =
        powf(max(0.0f, cosAngle), 6.0f) *
        max(0.0f, 1.0f - fabsf(sunHeight) * 4.0f);
    sky += horizonGlow * float3(0.8f, 0.3f, 0.05f) * sunIntensity;

    if (sunHeight < 0.0f)
    {
        float starMask = max(0.0f, -sunHeight);
        float v = sinf(dot(d, float3(127.1f, 311.7f, 74.4f))) * 43758.5453f;
        float star = v - floorf(v);
        star = smoothstep(0.997f, 1.0f, star);
        sky += star * starMask * float3(0.9f, 0.9f, 1.0f);
    }

    return sky;
}

// ===========================================================================
//  RebuildSkyCache
//
//  Bakes the blended sky into a 1024×512 float4 table.
//
//
//  To enable OpenMP in MSVC: Project → Properties → C/C++ → Language →
//  OpenMP Support → Yes (/openmp).
//  GCC/Clang: add -fopenmp to both compile and link flags.
// ===========================================================================
void Sky::RebuildSkyCache()
{
    // skyCache is already sized in the constructor; this is a no-op after
    // the first frame but kept for safety.
    skyCache.resize(SKY_W * SKY_H);

#pragma omp parallel for schedule(static)
    for (int y = 0; y < SKY_H; y++)
    {
        for (int x = 0; x < SKY_W; x++)
        {
            // UV with half-pixel offset so each sample hits the pixel centre.
            float u = (x + 0.5f) / (float)SKY_W;
            float v = (y + 0.5f) / (float)SKY_H;

            // Convert UV back to a unit direction (equirectangular).
            float phi = u * 2.0f * PI - PI;   // [-π, π]
            float theta = v * PI;                // [0, π]

            float sinT = sinf(theta);
            float3 dir = {
                sinT * cosf(phi),
                cosf(theta),
                sinT * sinf(phi)
            };

            float3 colour = GetSkyColorUncached(dir);

            SkyPixel& p = skyCache[y * SKY_W + x];
            p.r = colour.x;
            p.g = colour.y;
            p.b = colour.z;
            // p._pad intentionally unused
        }
    }

    skyCacheDirty = false;
    lastBuiltTime = timeOfDay;
}

// ===========================================================================
//  GetSkyColor  — HOT PATH, called once per ray that misses geometry
// ===========================================================================
float3 Sky::GetSkyColor(const float3& dir) const
{
    // --- direction → UV (equirectangular) ---
    float phi = FastAtan2(dir.z, dir.x);                    // [-π, π]
    float theta = FastAcos(clamp(dir.y, -1.0f, 1.0f));     // [0,  π]

    float u = (phi + PI) / (2.0f * PI);    // [0, 1]
    float v = theta / PI;                        // [0, 1]

    // --- continuous pixel coordinates (pixel centre at +0.5) ---
    float fx = u * (float)SKY_W - 0.5f;
    float fy = v * (float)SKY_H - 0.5f;

    int   ix = (int)floorf(fx);
    int   iy = (int)floorf(fy);
    float tx = fx - (float)ix;    // horizontal blend weight [0,1)
    float ty = fy - (float)iy;    // vertical   blend weight [0,1)

    // --- 2×2 neighbour indices ---
    // Horizontal wrap: bitwise AND valid because SKY_W is a power of two.
    // Vertical clamp: no wrap — the sky has distinct top and bottom poles.
    constexpr int MASK_W = SKY_W - 1;   // 0x3FF  (1023)
    constexpr int MASK_H = SKY_H - 1;   // 0x1FF  (511)

    int x0 = (ix)&MASK_W;
    int x1 = (ix + 1) & MASK_W;
    int y0 = max(0, min(iy, MASK_H));
    int y1 = max(0, min(iy + 1, MASK_H));

    // --- fetch four neighbours ---
    const SkyPixel& p00 = skyCache[y0 * SKY_W + x0];
    const SkyPixel& p10 = skyCache[y0 * SKY_W + x1];
    const SkyPixel& p01 = skyCache[y1 * SKY_W + x0];
    const SkyPixel& p11 = skyCache[y1 * SKY_W + x1];

    // --- bilinear blend (no branches) ---
    float itx = 1.0f - tx;
    float ity = 1.0f - ty;

    float w00 = itx * ity;
    float w10 = tx * ity;
    float w01 = itx * ty;
    float w11 = tx * ty;

    return float3(
        p00.r * w00 + p10.r * w10 + p01.r * w01 + p11.r * w11,
        p00.g * w00 + p10.g * w10 + p01.g * w01 + p11.g * w11,
        p00.b * w00 + p10.b * w10 + p01.b * w01 + p11.b * w11
    );
}

// ===========================================================================
//  GetSkyColorUncached  — called only from RebuildSkyCache, not per ray
// ===========================================================================
float3 Sky::GetSkyColorUncached(const float3& dir) const
{
    float3 procedural = GetProceduralSky(dir);

    if (!hdrSky.data)
        return procedural;

    float3 hdr = SampleHDR(dir);
    return lerp(procedural, hdr, cachedHdrBlend);
}