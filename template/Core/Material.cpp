#include "template.h"
#include "Material.h"
#include <algorithm>

float3 Material::EvaluateBRDF(const float3& wo, const float3& wi, const float3& N) const
{
    static constexpr double Pi = 3.14159265f;

    if (type != MaterialType::Microfacet)
        return albedo / Pi;

    float3 H = normalize(wi + wo);

    float NdotL = std::max(dot(N, wi), 0.0f);
    float NdotV = std::max(dot(N, wo), 0.0f);
    float NdotH = std::max(dot(N, H), 0.0f);
    float VdotH = std::max(dot(wo, H), 0.0f);

    if (NdotL <= 0.0f || NdotV <= 0.0f)
        return float3{ 0,0,0 };

    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float denom = NdotH * NdotH * (alpha2 - 1.0f) + 1.0f;
    float D = alpha2 / (Pi * denom * denom);

    float k = (roughness + 1.0f) * (roughness + 1.0f) / 8.0f;
    float G1_V = NdotV / (NdotV * (1.0f - k) + k);
    float G1_L = NdotL / (NdotL * (1.0f - k) + k);
    float G = G1_V * G1_L;

    float3 F = F0 + (float3{ 1.0f,1.0f,1.0f } - F0) * powf(1.0f - VdotH, 5.0f);

    float3 kD = (float3{ 1.0f,1.0f,1.0f } - F) * (1.0f - metallic);
    float3 diffuse = kD * albedo / Pi;

    float3 specular = (D * G * F) / (4.0f * NdotV * NdotL + 0.0001f);

    return diffuse + specular;
}
