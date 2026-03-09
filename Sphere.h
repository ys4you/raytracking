#pragma once

/// @brief  Represents a sphere primitive in the scene.
///
/// Spheres are stored separately from the voxel grid and intersected
/// either via brute-force or a tinybvh BVH depending on count.
struct Sphere
{
    float3 center;  // world-space centre position
    float  radius;  // sphere radius in world units
    uint   material; // index into the scene's material array
};