#pragma once

//#define STB_IMAGE_IMPLEMENTATION
//#include "stb_image.h"

struct VoxTexture {
    int      width = 0, height = 0;
    uint8_t* data = nullptr;   // RGBA8, loaded via stb_image
    float3 Sample(float u, float v) const;

    //void LoadPaletteTexture(int paletteIndex, const char* pngPath)
    //{
    //    int w, h, ch;
    //    uint8_t* pixels = stbi_load(pngPath, &w, &h, &ch, 4);
    //    if (!pixels) return;
    //    gPaletteTextures[paletteIndex] = { w, h, pixels };
    //}
};

extern VoxTexture gPaletteTextures[256];  // slot i → palette index i



