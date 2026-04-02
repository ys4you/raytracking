#include "template.h"
#include "VoxelFactory.h"
#include "VoxelObject.h"
#include "VoxelInstance.h"
#include "Core/Material.h"
#include <ogt_vox.h>
#include <cmath>

int VoxelFactory::CreateObject(Tmpl8::Scene& scene, const uint sizeX, const uint sizeY, const uint sizeZ,
    const std::vector<uint8_t>& voxels)
{
    const VoxelObject obj(sizeX, sizeY, sizeZ, voxels);
    const int index = (int)scene.voxelObjects.size();
    scene.voxelObjects.push_back(obj);
    return index;
}

void VoxelFactory::CreateInstance(
    Tmpl8::Scene& scene,
    const int objectIndex,
    const float3 position,
    const float3 rotation,
    const float3 scale,
    const float3 pivot)
{
    const VoxelObject& obj = scene.voxelObjects[objectIndex];

    const float invWS = 1.0f / WORLDSIZE;
    const float3 wsPos = position * invWS;
    const float3 wsScale = scale * invWS;

    float3 localPivot;
    if (pivot.x == 0 && pivot.y == 0 && pivot.z == 0)
    {
        localPivot = float3(
            (float)obj.sizeX * 0.5f,
            (float)obj.sizeY * 0.5f,
            (float)obj.sizeZ * 0.5f
        );
    }
    else
    {
        localPivot = pivot;
    }

    static constexpr float DEG2RAD = 3.14159265f / 180.0f;
    float3 rotRad = rotation * DEG2RAD;
    // Convert VOX-up authored assets into the engine's forward convention.
    rotRad.x -= 3.14159265f / 2.0f;

    VoxelInstance inst(objectIndex, wsPos, rotRad, wsScale, localPivot);
    scene.voxelInstances.push_back(inst);
}

void VoxelFactory::FromVoxTransform(const ogt_vox_transform& T,
    float3& outPos, float3& outRot, float3& outScale, const float3 pivot)
{
    (void)pivot;
    outPos = float3(T.m30, T.m31, T.m32);
    outScale.x = sqrtf(T.m00 * T.m00 + T.m01 * T.m01 + T.m02 * T.m02);
    outScale.y = sqrtf(T.m10 * T.m10 + T.m11 * T.m11 + T.m12 * T.m12);
    outScale.z = sqrtf(T.m20 * T.m20 + T.m21 * T.m21 + T.m22 * T.m22);
    outRot.x = atan2f(T.m21 / outScale.z, T.m22 / outScale.z);
    outRot.y = atan2f(-T.m20 / outScale.z,
        sqrtf(T.m21 * T.m21 + T.m22 * T.m22) / outScale.z);
    outRot.z = atan2f(T.m10 / outScale.y, T.m00 / outScale.x);
}

void VoxelFactory::FlattenInstance(
    Tmpl8::Scene& scene,
    const int objectIndex,
    const float3 position,
    const float3 rotation,
    const float3 scale)
{
    const VoxelObject& obj = scene.voxelObjects[objectIndex];
    const float cx = cosf(rotation.x), sx = sinf(rotation.x);
    const float cy = cosf(rotation.y), sy = sinf(rotation.y);
    const float cz = cosf(rotation.z), sz = sinf(rotation.z);
    for (uint z = 0; z < obj.sizeZ; z++)
        for (uint y = 0; y < obj.sizeY; y++)
            for (uint x = 0; x < obj.sizeX; x++)
            {
                uint8_t ci = obj.voxels[x + y * obj.sizeX + z * obj.sizeX * obj.sizeY];
                if (ci == 0) continue;
                const float lx = ((float)x - obj.sizeX * 0.5f) * scale.x;
                const float ly = ((float)y - obj.sizeY * 0.5f) * scale.y;
                const float lz = ((float)z - obj.sizeZ * 0.5f) * scale.z;
                const float rx = cy * lx + sy * lz;
                const float ry = ly;
                const float rz = -sy * lx + cy * lz;
                const float rx2 = rx;
                const float ry2 = cx * ry - sx * rz;
                const float rz2 = sx * ry + cx * rz;
                const float rx3 = cz * rx2 - sz * ry2;
                const float ry3 = sz * rx2 + cz * ry2;
                const float rz3 = rz2;
                const int gx = (int)roundf(rx3 + position.x);
                const int gy = (int)roundf(ry3 + position.y);
                const int gz = (int)roundf(rz3 + position.z);
                if ((uint)gx >= WORLDSIZE || (uint)gy >= WORLDSIZE || (uint)gz >= WORLDSIZE)
                    continue;
                scene.SetVoxel(gx, gy, gz, ci);
            }
}
