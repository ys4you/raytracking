#pragma once

class VoxLoader
{
public:
    static bool Load(const char* path, uint* grid, int gridSize, Material* materials);
private:
    //void BuildColorLUT(const ogt_vox_scene* scene);
    //void PlaceInstance(const ogt_vox_instance& inst,
    //    const ogt_vox_scene* scene,
    //    uint* grid, int gridSize);

    //uint colorLUT[256] = {};
};
