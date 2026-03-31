#pragma once

enum class MaterialType : uint8_t { Lambertian = 0, Metal = 1, Dielectric = 2, Emissive = 3, Microfacet = 4 };

struct Material
{
    MaterialType type = MaterialType::Lambertian;
    float3  albedo = { 0.8f, 0.8f, 0.8f };
    float roughness = 0.5f;
    float metallic = 0.0f;
    float ior = 1.5f;
    float3  emission = { 0, 0, 0 };
    float emissionStr = 0.0f;
    float3 F0;

    float3 EvaluateBRDF(const float3& wo, const float3& wi, const float3& N) const;
};
