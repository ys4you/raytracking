#include "template.h"
#include "Sky.h"


Sky::Sky()
{
    sun.direction = normalize(float3(0, -1, 0.2f));
    sun.color = float3(1.0f, 0.95f, 0.8f);
    moon.direction = normalize(float3(0, 1, 0.2f));
    moon.color = float3(0.1f, 0.15f, 0.25f);

    skyCache.resize(SKY_W * SKY_H);

    bool ok = hdrSky.Load("assets/order_sky.hdr");
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

        if (fabsf(timeOfDay - lastBuiltTime) > TIME_DIRTY_THRESHOLD)
            skyCacheDirty = true;
    }

    UpdateLights();

    if (skyCacheDirty)
        RebuildSkyCache();
}

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

float3 Sky::SampleHDR(const float3& dir) const
{
    if (!hdrSky.data)
        return float3(1, 0, 1);

    float3 d = normalize(dir);

    float theta = FastAcos(clamp(d.y, -1.0f, 1.0f));
    float phi = FastAtan2(d.z, d.x);

    float u = (phi + PI) / (2.0f * PI);
    float v = 1.0f - (theta / PI);

    return hdrSky.Sample(u, v);
}

float3 Sky::GetProceduralSky(const float3& dir) const
{
    float3 d = normalize(dir);

    const float3& sunDir = cachedSunDir;
    const float   sunHeight = cachedSunHeight;

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

void Sky::RebuildSkyCache()
{
    skyCache.resize(SKY_W * SKY_H);

#pragma omp parallel for schedule(static)
    for (int y = 0; y < SKY_H; y++)
    {
        for (int x = 0; x < SKY_W; x++)
        {
            float u = (x + 0.5f) / (float)SKY_W;
            float v = (y + 0.5f) / (float)SKY_H;

            float phi = u * 2.0f * PI - PI;
            float theta = v * PI;

            float sinT = sinf(theta);
            float3 dir = {
                sinT * cosf(phi),
                cosf(theta),
                sinT * sinf(phi)
            };

            float3 colour = GetSkyColorUncached(dir);

            float exposure = 1.2f;
            colour = float3(
                1.0f - expf(-colour.x * exposure),
                1.0f - expf(-colour.y * exposure),
                1.0f - expf(-colour.z * exposure)
            );

            colour = float3(
                powf(colour.x, 1.0f / 2.2f),
                powf(colour.y, 1.0f / 2.2f),
                powf(colour.z, 1.0f / 2.2f)
            );

            float noise = sinf(dot(dir, float3(12.9898f, 78.233f, 45.164f))) * 43758.5453f;
            noise = noise - floorf(noise);
            colour += (noise - 0.5f) * 0.002f;

            colour = clamp(colour, 0.0f, 1.0f);

            SkyPixel& p = skyCache[y * SKY_W + x];
            p.r = colour.x;
            p.g = colour.y;
            p.b = colour.z;
        }
    }

    skyCacheDirty = false;
    lastBuiltTime = timeOfDay;
}

float3 Sky::GetSkyColor(const float3& dir) const
{
    // Convert direction to equirectangular UV.
    float phi = FastAtan2(dir.z, dir.x);
    float theta = FastAcos(clamp(dir.y, -1.0f, 1.0f));

    float u = (phi + PI) / (2.0f * PI);
    float v = theta / PI;

    float fx = u * (float)SKY_W - 0.5f;
    float fy = v * (float)SKY_H - 0.5f;

    int   ix = (int)floorf(fx);
    int   iy = (int)floorf(fy);
    float tx = fx - (float)ix;
    float ty = fy - (float)iy;

    // Bit masks are valid because SKY_W and SKY_H are powers of two.
    constexpr int MASK_W = SKY_W - 1;
    constexpr int MASK_H = SKY_H - 1;

    int x0 = (ix)&MASK_W;
    int x1 = (ix + 1) & MASK_W;
    int y0 = max(0, min(iy, MASK_H));
    int y1 = max(0, min(iy + 1, MASK_H));

    const SkyPixel& p00 = skyCache[y0 * SKY_W + x0];
    const SkyPixel& p10 = skyCache[y0 * SKY_W + x1];
    const SkyPixel& p01 = skyCache[y1 * SKY_W + x0];
    const SkyPixel& p11 = skyCache[y1 * SKY_W + x1];

    // Bilinear blend from the 2x2 neighborhood.
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

float3 Sky::GetSkyColorUncached(const float3& dir) const
{
    float3 procedural = GetProceduralSky(dir);

    if (!hdrSky.data)
        return procedural;

    float3 hdr = SampleHDR(dir);

    float3 blended = lerp(procedural, hdr, cachedHdrBlend);

    return blended;
}
