#pragma once

#include "stb_image.h"


struct HDRCubemap
{
    int width = 0;
    int height = 0;
    int channels = 0;
    float* data = nullptr;

    bool Load(const char* path)
    {
        stbi_set_flip_vertically_on_load(true);
        data = stbi_loadf(path, &width, &height, &channels, 3);
        return data != nullptr;
    }

float3 Sample(float u, float v) const
{
    if (!data || width == 0 || height == 0)
        return float3(0.0f); // debug black

    int x = clamp(int(u * width), 0, width - 1);
    int y = clamp(int(v * height), 0, height - 1);
    int idx = (y * width + x) * 3;
    return float3(data[idx], data[idx + 1], data[idx + 2]);
}

    void Free() { if (data) stbi_image_free(data); data = nullptr; }
};
