#pragma once

#include "stb_image.h"

// Simple HDR texture loader + sampler for environment maps.
// This loads a floating-point image (HDR/EXR/etc.) and allows
// sampling it using normalized UV coordinates.
struct HDRCubemap
{
    // Image resolution
    int width = 0;
    int height = 0;

    // Number of channels in the loaded image
    int channels = 0;

    // Pointer to floating point pixel data (RGBRGBRGB...)
    float* data = nullptr;

    // Load an HDR image from disk
    bool Load(const char* path)
    {
        // Flip vertically so the texture matches OpenGL-style UVs
        stbi_set_flip_vertically_on_load(true);

        // Load floating-point image data (HDR)
        // The final parameter forces 3 channels (RGB)
        data = stbi_loadf(path, &width, &height, &channels, 3);

        // Return true if loading succeeded
        return data != nullptr;
    }

    // Sample the HDR texture using normalized coordinates (u,v)
    // u = horizontal coordinate [0..1]
    // v = vertical coordinate [0..1]
    float3 Sample(float u, float v) const
    {
        // If texture not loaded, return black (useful debug fallback)
        if (!data || width == 0 || height == 0)
            return float3(0.0f);

        // Convert normalized UV to pixel coordinates
        int x = clamp(int(u * width), 0, width - 1);
        int y = clamp(int(v * height), 0, height - 1);

        // Compute index in the linear pixel buffer
        // Each pixel has 3 floats: R, G, B
        int idx = (y * width + x) * 3;

        // Return sampled color
        return float3(data[idx], data[idx + 1], data[idx + 2]);
    }

    // Free allocated image memory
    void Free()
    {
        if (data)
            stbi_image_free(data);

        data = nullptr;
    }
};