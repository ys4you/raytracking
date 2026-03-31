// ============================================================
// OrbitCloudScene.h — Sterile Choreography Edition
// ============================================================
// A hypnotic kinetic sculpture:
// - Smooth orbit drift
// - Gentle height oscillation
// - Soft global breathing
// - Subtle beat flashes (not strong pulses)
// - Occasional micro-alignment moments
//
// Designed to be visually appealing for any duration.
// Side Order palette, clinical lighting.
// ============================================================

#pragma once
#include "SceneManager.h"
#include "VoxelFactory.h"
#include "VoxelObject.h"
#include <vector>
#include <cstring>
#include <algorithm>
#include <cmath>

namespace GameScenes
{

    static constexpr uint8_t PAL_OC_MISS = 238;  // Near White
    static constexpr uint8_t PAL_OC_HIT = 239;  // Muted Teal
    static constexpr uint8_t PAL_OC_FLASH = 240;  // Emissive

    static constexpr float OC_BPM = 79.0f;
    static constexpr float OC_BEAT = 60.0f / OC_BPM;
    static constexpr float OC_RAD2DEG = 180.0f / 3.14159265f;

    static constexpr int OC_COUNT = 24;


    // ------------------------------------------------------------
    // Cube data
    // ------------------------------------------------------------
    struct OrbitCube
    {
        float orbitRadius;
        float orbitSpeed;
        float orbitPhase;
        float heightOffset;
        float cubeSize;
        bool  isHit;

        // Smooth choreography additions
        float baseHeight;
        float heightOscPhase;
    };


    // ------------------------------------------------------------
    // Scene state
    // ------------------------------------------------------------
    struct OrbitCloudState
    {
        OrbitCube cubes[OC_COUNT] = {};

        float beatTimer = 0.0f;
        int   beatCount = 0;
        float beatPhase = 0.0f;
        bool  inFlash = false;

        // Smooth global breathing
        float globalBreath = 0.0f;

        // Group rotation
        float angleX = 0, angleY = 0, angleZ = 0;
        float speedX = 0.06f, speedY = 0.12f, speedZ = 0.04f;

        // Ray simulation
        float rayTimer = 0.0f;
        float stepInterval = 0.25f;
        int   stepCount = 0;

        bool  paused = false;
        int   objBase = -1;
        bool  needsInit = true;


        // ------------------------------------------------------------
        // Seed initial cube data
        // ------------------------------------------------------------
        void Seed()
        {
            for (int i = 0; i < OC_COUNT; i++)
            {
                OrbitCube& c = cubes[i];

                int shell = i % 3;
                c.orbitRadius = 18.0f + shell * 10.0f + (rand() % 5);
                c.orbitSpeed = 0.25f + (rand() % 100) * 0.004f;
                if (rand() % 2) c.orbitSpeed = -c.orbitSpeed;

                c.orbitPhase = (float)(rand() % 628) * 0.01f;

                c.baseHeight = -10.0f + (rand() % 20);
                c.heightOffset = c.baseHeight;

                c.heightOscPhase = (rand() % 628) * 0.01f;

                c.cubeSize = 5.0f + (rand() % 4);
                c.isHit = false;
            }
        }


        // ------------------------------------------------------------
        // Ray simulation (kept subtle)
        // ------------------------------------------------------------
        void SimulateRay()
        {
            float theta = (rand() % 628) * 0.01f;
            float phi = (rand() % 314) * 0.01f - 1.5708f;

            float rx = cosf(phi) * cosf(theta);
            float ry = sinf(phi);
            float rz = cosf(phi) * sinf(theta);

            for (int i = 0; i < OC_COUNT; i++)
            {
                OrbitCube& c = cubes[i];

                float px = cosf(c.orbitPhase) * c.orbitRadius;
                float py = c.heightOffset;
                float pz = sinf(c.orbitPhase) * c.orbitRadius;

                float cx = ry * pz - rz * py;
                float cy = rz * px - rx * pz;
                float cz = rx * py - ry * px;

                float dist = sqrtf(cx * cx + cy * cy + cz * cz);

                c.isHit = (dist < c.cubeSize * 1.35f);
            }
        }


        // ------------------------------------------------------------
        // Smooth rotation
        // ------------------------------------------------------------
        void TickRotation(float dtMs)
        {
            float dt = dtMs * 0.001f;
            angleX += speedX * dt;
            angleY += speedY * dt;
            angleZ += speedZ * dt;

            if (angleX > 2 * PI) angleX -= 2 * PI;
            if (angleY > 2 * PI) angleY -= 2 * PI;
            if (angleZ > 2 * PI) angleZ -= 2 * PI;
        }


        // ------------------------------------------------------------
        // Main tick — smooth choreography
        // ------------------------------------------------------------
        void Tick(float dtMs)
        {
            if (paused) return;
            float dt = dtMs * 0.001f;

            // Continuous orbit drift
            for (int i = 0; i < OC_COUNT; i++)
            {
                OrbitCube& c = cubes[i];
                c.orbitPhase += c.orbitSpeed * dt;

                // Smooth height oscillation
                c.heightOscPhase += dt * 0.6f;
                c.heightOffset = c.baseHeight + sinf(c.heightOscPhase) * 2.0f;
            }

            // Global breathing (very subtle)
            globalBreath = 1.0f + 0.03f * sinf(beatTimer * 0.8f);

            // Beat tracking
            beatTimer += dt;
            beatPhase = fmodf(beatTimer / OC_BEAT, 1.0f);

            // Soft flash on beat
            int currentBeat = (int)(beatTimer / OC_BEAT);
            if (currentBeat > beatCount)
            {
                beatCount = currentBeat;
                inFlash = true;
            }

            if (inFlash)
            {
                if (beatPhase > 0.12f)
                    inFlash = false;
            }

            // Ray simulation (slow)
            rayTimer += dt;
            if (rayTimer >= stepInterval)
            {
                rayTimer -= stepInterval;
                SimulateRay();
            }
        }


        float3 GetRot() const { return float3(angleX, angleY, angleZ); }


        void Reset()
        {
            beatTimer = 0;
            beatCount = 0;
            beatPhase = 0;
            inFlash = false;

            for (int i = 0; i < OC_COUNT; i++)
                cubes[i].isHit = false;
        }
    };


    // ------------------------------------------------------------
    // Singleton accessor
    // ------------------------------------------------------------
    inline std::shared_ptr<OrbitCloudState>& GetOrbitCloudState()
    {
        static std::shared_ptr<OrbitCloudState> inst;
        return inst;
    }


    // ------------------------------------------------------------
    // Setup voxel objects + materials
    // ------------------------------------------------------------
    inline void SetupOrbitCloudCubes(Tmpl8::Scene& scene, OrbitCloudState& oc)
    {
        oc.objBase = (int)scene.voxelObjects.size();

        scene.voxelObjects.push_back(VoxelObject(1, 1, 1, { PAL_OC_MISS }));
        scene.voxelObjects.push_back(VoxelObject(1, 1, 1, { PAL_OC_HIT }));
        scene.voxelObjects.push_back(VoxelObject(1, 1, 1, { PAL_OC_FLASH }));

        auto mat = [](float3 c, float r) -> Material
            {
                Material m;
                m.type = MaterialType::Lambertian;
                m.albedo = c;
                m.roughness = r;
                return m;
            };

        scene.materials[MAT_COUNT + (PAL_OC_MISS - 1)] = mat(float3(0.96f), 0.30f);
        scene.materials[MAT_COUNT + (PAL_OC_HIT - 1)] = mat(float3(0.549f, 0.722f, 0.690f), 0.20f);

        Material m;
        m.type = MaterialType::Emissive;
        m.albedo = float3(1, 1, 1);
        m.emission = float3(1, 0.98f, 0.95f);
        m.emissionStr = 4.0f;
        m.roughness = 0.1f;
        scene.materials[MAT_COUNT + (PAL_OC_FLASH - 1)] = m;
    }


    // ------------------------------------------------------------
    // Sync — smooth choreography rendering
    // ------------------------------------------------------------
    inline void SyncOrbitCloud(
        const OrbitCloudState& oc,
        Tmpl8::Scene& scene,
        float3 rotRad)
    {
        scene.voxelInstances.clear();

        const float3 center(256, 240, 256);
        float3 rotDeg = rotRad * OC_RAD2DEG;

        mat4 rot = mat4::RotateX(rotRad.x)
            * mat4::RotateY(rotRad.y)
            * mat4::RotateZ(rotRad.z);

        auto orbit = [&](float3 p) -> float3
            {
                float3 o = p - center;
                float4 r = rot * float4(o, 1);
                return float3(r.x, r.y, r.z) + center;
            };

        for (int i = 0; i < OC_COUNT; i++)
        {
            const OrbitCube& c = oc.cubes[i];

            float3 pos(
                center.x + cosf(c.orbitPhase) * c.orbitRadius,
                center.y + c.heightOffset,
                center.z + sinf(c.orbitPhase) * c.orbitRadius
            );

            int objIdx;
            float scale = c.cubeSize * oc.globalBreath;

            if (oc.inFlash)
            {
                objIdx = oc.objBase + 2;
                scale *= 1.05f;
            }
            else if (c.isHit)
            {
                objIdx = oc.objBase + 1;
                scale *= 1.03f;
            }
            else
            {
                objIdx = oc.objBase + 0;
                scale *= 0.97f;
            }

            VoxelFactory::CreateInstance(
                scene, objIdx,
                orbit(pos),
                rotDeg,
                float3(scale), true
            );
        }

        scene.RebuildDirtyInstances();
    }


    // ------------------------------------------------------------
    // Scene definition
    // ------------------------------------------------------------
    inline SceneDef OrbitCloudShowcase()
    {
        SceneDef s;
        s.name = "Orbit";
        s.useVoxelGrid = true;

        s.voxObjects.push_back({
            "assets/Showcase/display_platform.vox",
            float3(256, 120, 256), float3(0), float3(1), true
            });

        s.camPos = float3(0.28f, 0.60f, 0.20f);
        s.camTarget = float3(0.50f, 0.47f, 0.50f);

        s.splinePoints = {
            float3(0.25f, 0.60f, 0.22f),
            float3(0.50f, 0.60f, 0.15f),
            float3(0.75f, 0.60f, 0.22f),
            float3(0.78f, 0.60f, 0.50f),
            float3(0.75f, 0.60f, 0.78f),
            float3(0.50f, 0.60f, 0.85f),
            float3(0.25f, 0.60f, 0.78f),
            float3(0.22f, 0.60f, 0.50f),
            float3(0.25f, 0.60f, 0.22f),
        };

        // Clinical lighting
        s.sky.sunDir = normalize(float3(0.3f, -0.5f, 0.2f));
        s.sky.sunColor = float3(1.0f, 0.98f, 0.95f);
        s.sky.sunIntensity = 2.0f;
        s.sky.timeOfDay = 0.35f;
        s.sky.animate = false;

        auto addL = [&](float3 p, float3 c)
            {
                PointLight l;
                l.position = p; l.color = c; l.enabled = true;
                s.pointLights.push_back(l);
            };
        addL(float3(0.50f, 0.90f, 0.50f), float3(0.9f, 0.88f, 0.85f));
        addL(float3(0.25f, 0.55f, 0.20f), float3(0.4f, 0.4f, 0.45f));
        addL(float3(0.75f, 0.55f, 0.80f), float3(0.4f, 0.4f, 0.45f));
        addL(float3(0.50f, 0.35f, 0.25f), float3(0.25f, 0.25f, 0.28f));

        auto& oc = GetOrbitCloudState();
        oc = std::make_shared<OrbitCloudState>();
        oc->Seed();

        s.tickCallback = [oc](SceneDef&, Tmpl8::Scene& scene,
            float dt, std::function<void()> reset)
            {
                scene.instancesShadows = false;
                if (oc->needsInit)
                {
                    oc->needsInit = false;
                    SetupOrbitCloudCubes(scene, *oc);
                    SyncOrbitCloud(*oc, scene, oc->GetRot());
                    if (reset) reset();
                    return;
                }

                oc->TickRotation(dt);
                oc->Tick(dt);
                SyncOrbitCloud(*oc, scene, oc->GetRot());
                if (reset) reset();
            };

        s.uiCallback = [oc](SceneDef&, Tmpl8::Scene& scene,
            std::function<void()> reset)
            {
                if (!ImGui::CollapsingHeader("TLAS Orbit Cloud", ImGuiTreeNodeFlags_DefaultOpen))
                    return;

                int hits = 0;
                for (int i = 0; i < OC_COUNT; i++)
                    if (oc->cubes[i].isHit) hits++;

                ImGui::Text("Beat: %d  |  %s  |  %d/%d hit  |  %d inst",
                    oc->beatCount,
                    oc->inFlash ? "FLASH" : "CALM",
                    hits, OC_COUNT,
                    (int)scene.voxelInstances.size());

                ImGui::ProgressBar(oc->beatPhase, ImVec2(-1, 3));

                ImGui::Spacing(); ImGui::Separator();

                if (ImGui::Button(oc->paused ? "  Play  " : " Pause  "))
                    oc->paused = !oc->paused;
                ImGui::SameLine();
                if (ImGui::Button("Reset"))
                {
                    oc->Reset();
                    oc->Seed();
                    SyncOrbitCloud(*oc, scene, oc->GetRot());
                    if (reset) reset();
                }
                ImGui::SameLine();
                if (ImGui::Button("Reseed"))
                {
                    oc->Seed();
                    SyncOrbitCloud(*oc, scene, oc->GetRot());
                    if (reset) reset();
                }

                ImGui::Spacing();

                if (ImGui::CollapsingHeader("Rotation"))
                {
                    ImGui::SliderFloat("X##oc", &oc->speedX, 0, 0.5f, "%.2f");
                    ImGui::SliderFloat("Y##oc", &oc->speedY, 0, 0.5f, "%.2f");
                    ImGui::SliderFloat("Z##oc", &oc->speedZ, 0, 0.5f, "%.2f");
                }
            };

        return s;
    }

} // namespace GameScenes
