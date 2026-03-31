#pragma once


struct VoxTexture {
    int      width = 0, height = 0;
    uint8_t* data = nullptr;
    float3 Sample(float u, float v) const;

};

extern VoxTexture gPaletteTextures[256];



