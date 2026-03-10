#pragma once
#include "stb_image.h"

// ---------------------------------------------------------------------------
// HDRCubemap  — equirectangular HDR environment map
// ---------------------------------------------------------------------------

struct HDRCubemap
{
    int    width = 0;
    int    height = 0;
    int    channels = 0;       // always 4 after Load()
    float* data = nullptr; // interleaved RGBA, 4 floats per pixel

    // -----------------------------------------------------------------------
    // Load
    //   Forces 4 channels so stride = width * 4 floats (16 bytes / pixel).
    //   Every pixel therefore starts at a multiple of 16 — aligned for SIMD.
    // -----------------------------------------------------------------------
    bool Load(const char* path)
    {
        stbi_set_flip_vertically_on_load(true);
        data = stbi_loadf(path, &width, &height, &channels, 4);
        channels = 4; // we forced it above; record the actual stride
        return data != nullptr;
    }

    // -----------------------------------------------------------------------
    // Sample  — bilinear lookup
    //   u ∈ [0,1] horizontal, v ∈ [0,1] vertical.
    //   Wraps horizontally (panoramic seam), clamps vertically (poles).
    // -----------------------------------------------------------------------
    float3 Sample(float u, float v) const
    {
        if (!data || width == 0 || height == 0)
            return float3(0.0f);

        // --- continuous pixel coordinates (pixel centre at 0.5) ---
        float fx = u * (float)width - 0.5f;
        float fy = v * (float)height - 0.5f;

        // --- integer base + fractional remainder ---
        int   ix = (int)floorf(fx);
        int   iy = (int)floorf(fy);
        float tx = fx - (float)ix;   // weight for right column  [0,1)
        float ty = fy - (float)iy;   // weight for bottom row    [0,1)

        // --- neighbour indices ---
        // Horizontal: wrap with modulo (panorama has a left-right seam).
        // Vertical  : clamp so we never read outside the image.
        int x0 = ((ix % width) + width) % width;  // wrap
        int x1 = ((ix + 1 % width) + width) % width;  // wrap
        int y0 = max(0, min(iy, height - 1));        // clamp
        int y1 = max(0, min(iy + 1, height - 1));        // clamp

        // --- fetch four texels (RGBA stride = 4) ---
        auto px = [&](int x, int y) -> float3
            {
                int base = (y * width + x) * 4;
                return float3(data[base], data[base + 1], data[base + 2]);
            };

        float3 c00 = px(x0, y0);   // top-left
        float3 c10 = px(x1, y0);   // top-right
        float3 c01 = px(x0, y1);   // bottom-left
        float3 c11 = px(x1, y1);   // bottom-right

        // --- bilinear blend ---
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