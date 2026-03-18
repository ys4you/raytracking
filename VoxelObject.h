#pragma once

/// @brief  Stores the raw voxel data for a single voxel object (BLAS).
///
/// Voxels are stored as a flat array of colour indices in X->Y->Z order.
/// Index 0 = empty; any other value maps to a palette entry.
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

    /// @brief  Look up a voxel by integer coordinate. No bounds check.
    __forceinline uint8_t Get(uint x, uint y, uint z) const
    {
        return voxels[x + y * sizeX + z * sizeX * sizeY];
    }
};