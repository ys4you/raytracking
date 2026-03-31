#pragma once

/// <summary>Stores voxel data for a single object used by instance tracing.</summary>
struct VoxelObject
{
    uint sizeX, sizeY, sizeZ;
    std::vector<uint8_t> voxels;

    VoxelObject() : sizeX(0), sizeY(0), sizeZ(0) {}

    VoxelObject(uint sx, uint sy, uint sz, const std::vector<uint8_t>& data)
        : sizeX(sx), sizeY(sy), sizeZ(sz), voxels(data)
    {
        if (voxels.size() != (size_t)sx * sy * sz)
            voxels.resize((size_t)sx * sy * sz, 0);
    }

    /// <summary>Looks up a voxel by integer coordinate without bounds checks.</summary>
    __forceinline uint8_t Get(uint x, uint y, uint z) const
    {
        return voxels[x + y * sizeX + z * sizeX * sizeY];
    }
};
