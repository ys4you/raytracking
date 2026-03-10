#pragma once

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
	float3 result(0);
    int samples = light.uSteps * light.vSteps;

    float invU = 1.0f / light.uSteps;
    float invV = 1.0f / light.vSteps;

    for (int v = 0; v < light.vSteps; ++v)
    {
        for (int u = 0; u < light.uSteps; ++u)
        {
	        constexpr float EPS = 0.05f;
	        // sample position on light
            const float fu = (u + 0.5f) * invU;
            const float fv = (v + 0.5f) * invV;

            float3 lightPos =
                light.corner +
                light.edge1 * fu +
                light.edge2 * fv;

            float3 L = lightPos - sp.position;

            float dist2 = dot(L, L);
            float dist = sqrt(dist2);

            float3 Ldir = L / dist;

            // one-sided emission
            if (dot(light.normal, -Ldir) <= 0.0f)
                continue;

            Ray shadowRay(
                sp.position + sp.normal * EPS,
                Ldir,
                dist - EPS
            );

            if (scene.IsOccluded(shadowRay))
                continue;

            const float ndotl = max(0.0f, dot(sp.normal, Ldir));

            float attenuation = 1.0f / dist2;

            result += light.color * ndotl * attenuation;
        }
    }

    return result / static_cast<float>(samples);
}