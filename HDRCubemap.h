#pragma once
#include "stb_image.h"

struct HDRCubemap
{
    int    width = 0;
    int    height = 0;
    int    channels = 0;
    float* data = nullptr;

    float FindMaxValue() const
    {
        if (!data) return 0.0f;

        float maxVal = 0.0f;

        int total = width * height * 4;

        for (int i = 0; i < total; i++)
        {
            if (data[i] > maxVal)
                maxVal = data[i];
        }

        return maxVal;
    }

    bool Load(const char* path)
    {
        stbi_set_flip_vertically_on_load(true);
        data = stbi_loadf(path, &width, &height, &channels, 4);

        printf("Max HDR value: %f\n", FindMaxValue());
        channels = 4;
        return data != nullptr;
    }

    float3 Sample(float u, float v) const
    {
        if (!data || width == 0 || height == 0)
            return float3(0.0f);

        float fx = u * (float)width - 0.5f;
        float fy = v * (float)height - 0.5f;

        int   ix = (int)floorf(fx);
        int   iy = (int)floorf(fy);
        float tx = fx - (float)ix;
        float ty = fy - (float)iy;

        int x0 = ((ix % width) + width) % width;
        int x1 = ((ix + 1) % width + width) % width;
        int y0 = max(0, min(iy, height - 1));
        int y1 = max(0, min(iy + 1, height - 1));

        auto px = [&](int x, int y) -> float3
            {
                int base = (y * width + x) * 4;
                return float3(data[base], data[base + 1], data[base + 2]);
            };

        float3 c00 = px(x0, y0);
        float3 c10 = px(x1, y0);
        float3 c01 = px(x0, y1);
        float3 c11 = px(x1, y1);

        float3 top = lerp(c00, c10, tx);
        float3 bot = lerp(c01, c11, tx);
        return lerp(top, bot, ty);
    }

    void Free()
    {
        if (data) stbi_image_free(data);
        data = nullptr;
    }
};