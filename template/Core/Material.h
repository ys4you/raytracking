#pragma once

enum class MaterialType : uint8_t {
    Lambertian = 0, Metal = 1, Dielectric = 2, Emissive = 3
};

struct Material {
    MaterialType type = MaterialType::Lambertian;
    float3  albedo = { 0.8f, 0.8f, 0.8f };
    float roughness = 0.5f;    // fuzz for metals, surface roughness
    float metallic = 0.0f;    // 0 = dielectric, 1 = conductor
    float ior = 1.5f;    // index of refraction (dielectrics)
    float3  emission = { 0, 0, 0 };
    float emissionStr = 0.0f;
};