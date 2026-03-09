#pragma once

/// @brief  Stores the raw voxel data for a single voxel object (BLAS).
///
/// Voxels are stored as a flat array of colour indices in X→Y→Z order.
/// Index 0 = empty; any other value maps to a palette entry.
/// This is the source data — for placing it in the world, use
/// VoxelFactory::FlattenInstance or VoxelFactory::CreateInstance.
struct VoxelObject
{
    uint sizeX, sizeY, sizeZ;       // dimensions of the voxel grid
    std::vector<uint8_t> voxels;    // colour indices; 0 = empty voxel

    /// @brief  Default constructor — creates an empty, zero-sized object.
    VoxelObject() : sizeX(0), sizeY(0), sizeZ(0) {}

    /// @brief  Constructs a VoxelObject from explicit dimensions and voxel data.
    ///
    /// If the supplied data vector does not match sx*sy*sz, it is resized
    /// and zero-filled to avoid out-of-bounds reads later.
    ///
    /// @param sx/sy/sz  Grid dimensions along each axis.
    /// @param data      Flat array of colour indices in X→Y→Z order.
    VoxelObject(uint sx, uint sy, uint sz, const std::vector<uint8_t>& data)
        : sizeX(sx), sizeY(sy), sizeZ(sz), voxels(data)
    {
        // Ensure the voxel array exactly matches the declared dimensions
        if (voxels.size() != sx * sy * sz)
            voxels.resize(sx * sy * sz, 0);
    }
};