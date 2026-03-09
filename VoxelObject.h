#pragma once

struct VoxelObject
{
    uint sizeX, sizeY, sizeZ;
    std::vector<uint8_t> voxels; // voxel color indices

    VoxelObject() : sizeX(0), sizeY(0), sizeZ(0) {}

    VoxelObject(uint sx, uint sy, uint sz, const std::vector<uint8_t>& data)
        : sizeX(sx), sizeY(sy), sizeZ(sz), voxels(data)
    {
        if (voxels.size() != sx * sy * sz)
            voxels.resize(sx * sy * sz, 0);
    }
};
