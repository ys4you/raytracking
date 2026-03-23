#pragma once
#include <xmmintrin.h>

inline float rsqrt(float x)
{
    __m128 a = _mm_set_ss(x);
    __m128 r = _mm_rsqrt_ss(a);
    return _mm_cvtss_f32(r);
}

struct AreaLight
{
    float3 corner;
    float3 edge1;
    float3 edge2;
    float3 normal;

    float3 color;

    int uSteps = 4;
    int vSteps = 4;

    bool enabled = true;

    AreaLight() = default;

    AreaLight(const float3& c, const float3& e1, const float3& e2,
        const float3& col, int u, int v)
        : corner(c), edge1(e1), edge2(e2), color(col), uSteps(u), vSteps(v)
    {
        normal = normalize(cross(edge1, edge2));
    }
};

inline float3 IlluminateArea(
    const AreaLight& light,
    const ShadingPoint& sp,
    const Scene& scene)
{
    constexpr float EPS = 0.05f;

    float3 result(0.0f);

    constexpr int samples = 4;

    for (int i = 0; i < samples; ++i)
    {
        float fu = RandomFloat();
        float fv = RandomFloat();

        float3 lightPos =
            light.corner +
            light.edge1 * fu +
            light.edge2 * fv;

        float3 L = lightPos - sp.position;

        float dist2 = dot(L, L);

        if (dist2 <= 1e-6f)
            continue;

        float invDist = rsqrt(dist2);
        float3 Ldir = L * invDist;

        if (dot(light.normal, -Ldir) <= 0.0f)
            continue;

        float ndotl = dot(sp.normal, Ldir);
        if (ndotl <= 0.0f)
            continue;

        Ray shadowRay(
            sp.position + sp.normal * EPS,
            Ldir,
            sqrt(dist2) - EPS // keep this simple & safe
        );

        if (scene.IsOccluded(shadowRay))
            continue;

        float attenuation = invDist * invDist;

        result += light.color * (ndotl * attenuation);
    }

    return result * (1.0f / samples);
}