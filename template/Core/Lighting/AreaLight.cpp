#include "template.h"
#include "AreaLight.h"

#include "../ShadingPoint.h"



AreaLight::AreaLight(const float3& c, const float3& e1, const float3& e2, const float3& col, int u, int v)
{
    corner = c;
    edge1 = e1;
    edge2 = e2;

    // Normal is perpendicular to the area light surface
    normal = normalize(cross(edge1, edge2));

    color = col;
    uSteps = u;
    vSteps = v;
}
float3 AreaLight::Illuminate(const ShadingPoint& sp, Scene& scene) const
{
    const float EPS = 0.05f;
    float3 result(0.0f);
    int samples = uSteps * vSteps;

    for (int v = 0; v < vSteps; ++v)
    {
        for (int u = 0; u < uSteps; ++u)
        {
            float3 lightPos = SamplePoint(u, v);

            float3 L = lightPos - sp.position;
            float dist = length(L);
            float3 Ldir = normalize(L);

            // One-sided emission check
            if (dot(normal, -Ldir) <= 0.0f)
                continue;

            // Shadow ray
            Ray shadowRay(
                sp.position + sp.normal * EPS,
                Ldir,
                dist - EPS
            );

            if (scene.IsOccluded(shadowRay))
                continue;

            float ndotl = max(0.0f, dot(sp.normal, Ldir));
            float attenuation = 1.0f / (dist * dist);

            result += (color * intensity )* sp.albedo * ndotl * attenuation;
        }
    }

    return result / float(samples);
}
