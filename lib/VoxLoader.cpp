#include "template.h"

#define OGT_VOX_IMPLEMENTATION
#include "ogt_vox.h"

#include "VoxLoader.h"
#include "VoxelFactory.h"
#include "Core/Material.h"

#include <stdio.h>
#include <string.h>
#include <algorithm>

bool VoxLoader::Load(const char* path, Tmpl8::Scene& scene)
{
    const bool VERBOSE_LOG = true;

    FILE* f = fopen(path, "rb");
    if (!f) { printf("[VoxLoader] Could not open: %s\n", path); return false; }

    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    rewind(f);
    if (VERBOSE_LOG) printf("[VoxLoader] File size: %ld bytes\n", fileSize);

    unsigned char* buffer = new unsigned char[fileSize];
    fread(buffer, 1, fileSize, f);
    fclose(f);

    const ogt_vox_scene* voxScene = ogt_vox_read_scene(buffer, (unsigned int)fileSize);
    delete[] buffer;

    if (!voxScene) { printf("[VoxLoader] Failed to parse: %s\n", path); return false; }

    if (VERBOSE_LOG)
        printf("[VoxLoader] Models: %d, Instances: %d\n",
            voxScene->num_models, voxScene->num_instances);

    // --- Fill Scene materials from palette ---
    for (int i = 1; i < 256; i++)
    {
        const ogt_vox_rgba& c = voxScene->palette.color[i];
        const ogt_vox_matl& m = voxScene->materials.matl[i];

        Material& mat = scene.materials[MAT_COUNT + (i - 1)];
        mat.albedo = { c.r / 255.f, c.g / 255.f, c.b / 255.f };

        switch (m.type)
        {
        case ogt_matl_type_metal:
            mat.type = MaterialType::Metal;
            mat.metallic = 1.0f;
            mat.roughness = (m.content_flags & k_ogt_vox_matl_have_rough) ? m.rough : 0.1f;
            break;
        case ogt_matl_type_glass:
            mat.type = MaterialType::Dielectric;
            mat.ior = (m.content_flags & k_ogt_vox_matl_have_ior) ? m.ior : 1.5f;
            break;
        case ogt_matl_type_emit:
            mat.type = MaterialType::Emissive;
            mat.emission = mat.albedo;
            mat.emissionStr = (m.content_flags & k_ogt_vox_matl_have_emit) ? m.emit : 1.0f;
            break;
        default:
            mat.type = MaterialType::Lambertian;
            mat.roughness = 1.0f;
            break;
        }
    }

    // --- Create objects only (no grid placement) ---
    for (unsigned int inst = 0; inst < voxScene->num_instances; inst++)
    {
        const ogt_vox_instance& instance = voxScene->instances[inst];
        const ogt_vox_model* model = voxScene->models[instance.model_index];
        if (!model) continue;

        std::vector<uint8_t> voxels(model->voxel_data,
            model->voxel_data + model->size_x * model->size_y * model->size_z);

        int objIndex = VoxelFactory::CreateObject(scene,
            model->size_x, model->size_y, model->size_z, voxels);

        if (VERBOSE_LOG)
            printf("[VoxLoader] Created object %d from model %d (%d x %d x %d)\n",
                objIndex, instance.model_index,
                model->size_x, model->size_y, model->size_z);
    }

    ogt_vox_destroy_scene(voxScene);
    printf("[VoxLoader] Loaded '%s' — %d objects created.\n", path, (int)scene.voxelObjects.size());
    return true;
}