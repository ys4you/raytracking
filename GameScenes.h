#pragma once
#include "SceneManager.h"

#include "LivingCubeScene.h"
#include "InfinityMirrorScene.h"
#include "GyroscopeScene.h"
#include "BrickmapScene.h"
#include "PulseGridScene.h"
#include "OrbitCloudScene.h"
#include "HelixScene.h"
#include "OutroShowcase.h"

namespace GameScenes
{

    // ─────────────────────────────────────────────
    // Shared slow-rotation state for showcase scenes
    // ─────────────────────────────────────────────

    struct ShowcaseRotation
    {
        float angleX = 0.0f, angleY = 0.0f, angleZ = 0.0f;
        float speedX = 0.08f, speedY = 0.18f, speedZ = 0.05f;

        void Tick(float deltaTimeMs)
        {
            const float dt = deltaTimeMs * 0.001f;
            angleX = fmodf(angleX + speedX * dt, 2.0f * PI);
            angleY = fmodf(angleY + speedY * dt, 2.0f * PI);
            angleZ = fmodf(angleZ + speedZ * dt, 2.0f * PI);
        }

        float3 GetRotation() const { return float3(angleX, angleY, angleZ); }
    };

    inline std::shared_ptr<ShowcaseRotation>& SharedRotation()
    {
        static auto s = std::make_shared<ShowcaseRotation>();
        return s;
    }

    // ─────────────────────────────────────────────
    // Side Order lighting rig — shared by all showcase scenes
    // ─────────────────────────────────────────────

    inline void AddShowcaseLights(SceneDef& s)
    {
        // Clinical overhead
        s.pointLights.push_back({ float3(0.5f, 0.9f, 0.5f),
                                  float3(1.6f, 1.58f, 1.55f), true });
        // Front key — cool white
        s.pointLights.push_back({ float3(0.5f, 0.45f, 0.08f),
                                  float3(0.9f, 0.92f, 0.95f), true });
        // Left fill — muted teal
        s.pointLights.push_back({ float3(0.08f, 0.5f, 0.5f),
                                  float3(0.35f, 0.55f, 0.50f), true });
        // Right fill — pale lavender
        s.pointLights.push_back({ float3(0.92f, 0.5f, 0.5f),
                                  float3(0.45f, 0.40f, 0.55f), true });
        // Under-fill
        s.pointLights.push_back({ float3(0.5f, 0.12f, 0.5f),
                                  float3(0.18f, 0.18f, 0.20f), true });
        // Back rim
        s.pointLights.push_back({ float3(0.5f, 0.55f, 0.92f),
                                  float3(0.25f, 0.32f, 0.40f), true });
    }

    // ─────────────────────────────────────────────
    // Showcase base setup — sky, camera, lights, rotation callback
    // ─────────────────────────────────────────────

    inline void SetupShowcase(SceneDef& s)
    {
        s.camPos = float3(0.5f, 0.46f, 0.22f);
        s.camTarget = float3(0.5f, 0.42f, 0.5f);

        s.sky.sunDir = normalize(float3(0.1f, -0.8f, 0.2f));
        s.sky.sunColor = float3(1.0f, 0.98f, 0.96f);
        s.sky.sunIntensity = 0.8f;
        s.sky.timeOfDay = 0.28f;
        s.sky.animate = false;

        AddShowcaseLights(s);

        auto rot = SharedRotation();

        s.tickCallback = [rot](SceneDef&, Tmpl8::Scene& scene,
            float deltaTime, std::function<void()> resetAcc)
            {
                if (scene.voxelInstances.empty()) return;
                rot->Tick(deltaTime);
                scene.voxelInstances[0].rotation = rot->GetRotation();
                scene.voxelInstances[0].matricesDirty = true;
                scene.RebuildDirtyInstances();
                if (resetAcc) resetAcc();
            };

        s.uiCallback = [rot](SceneDef&, Tmpl8::Scene&, std::function<void()>)
            {
                if (ImGui::CollapsingHeader("Showcase Settings"))
                {
                    ImGui::SliderFloat("Speed X", &rot->speedX, 0.0f, 1.0f, "%.2f rad/s");
                    ImGui::SliderFloat("Speed Y", &rot->speedY, 0.0f, 1.0f, "%.2f rad/s");
                    ImGui::SliderFloat("Speed Z", &rot->speedZ, 0.0f, 1.0f, "%.2f rad/s");
                }
            };
    }

    // ─────────────────────────────────────────────
    // Helper: add the standard display platform
    // ─────────────────────────────────────────────

    inline void AddPlatform(SceneDef& s)
    {
        s.voxObjects.push_back({
            "assets/Showcase/display_platform.vox",
            float3(256, 120, 256),
            float3(0, 0, 0),
            float3(1, 1, 1), true
            });
    }

    // ─────────────────────────────────────────────
    // Helper: add an orbit spline around center
    // ─────────────────────────────────────────────

    inline void AddOrbitSpline(SceneDef& s, float radius, float baseY, float dip, float speed)
    {
        const float cx = 0.5f, cz = 0.5f;
        s.splinePoints = {
            float3(cx,              baseY,       cz - radius),
            float3(cx - radius * 0.7f, baseY - dip, cz - radius * 0.7f),
            float3(cx - radius,     baseY - dip * 2, cz),
            float3(cx - radius * 0.7f, baseY - dip, cz + radius * 0.7f),
            float3(cx,              baseY,       cz + radius),
            float3(cx + radius * 0.7f, baseY - dip, cz + radius * 0.7f),
            float3(cx + radius,     baseY - dip * 2, cz),
            float3(cx + radius * 0.7f, baseY - dip, cz - radius * 0.7f),
        };
        s.splineSpeed = speed;
        s.splineEnabled = true;
    }

    // ═════════════════════════════════════════════
    //  SCENE DEFINITIONS
    // ═════════════════════════════════════════════

    inline SceneDef CubeShowcase()
    {
        SceneDef s;
        s.name = "01 CUBE";
        SetupShowcase(s);
        AddPlatform(s);
        AddOrbitSpline(s, 0.30f, 0.46f, 0.02f, 0.012f);

        s.voxObjects.push_back({
            "assets/Showcase/display_cube.vox",
            float3(256, 240, 256),
            float3(0, 0, 0),
            float3(1, 1, 1)
            });

        return s;
    }

    inline SceneDef MengerShowcase()
    {
        SceneDef s;
        s.name = "02 MENGER";
        SetupShowcase(s);
        AddPlatform(s);
        AddOrbitSpline(s, 0.32f, 0.44f, 0.03f, 0.010f);

        // Slower rotation — let fractal detail read
        auto rot = SharedRotation();
        rot->speedX = 0.06f;
        rot->speedY = 0.14f;
        rot->speedZ = 0.04f;

        s.voxObjects.push_back({
            "assets/Showcase/menger_sponge.vox",
            float3(256, 245, 256),
            float3(0, 0, 0),
            float3(1, 1, 1)
            });

        return s;
    }

    // ─────────────────────────────────────────────
    // Sphere spawner UI (used by Physics scene)
    // ─────────────────────────────────────────────

    inline void SphereSpawnerUI(SceneDef& def, Tmpl8::Scene& scene, std::function<void()> resetAcc)
    {
        if (!ImGui::CollapsingHeader("Sphere Spawner")) return;

        SpawnerState& sp = def.spawner;

        ImGui::DragFloat3("Min", &sp.rangeMin.x, 0.01f, 0.0f, 1.0f, "%.2f");
        ImGui::DragFloat3("Max", &sp.rangeMax.x, 0.01f, 0.0f, 1.0f, "%.2f");
        sp.rangeMin.x = min(sp.rangeMin.x, sp.rangeMax.x - 0.01f);
        sp.rangeMin.y = min(sp.rangeMin.y, sp.rangeMax.y - 0.01f);
        sp.rangeMin.z = min(sp.rangeMin.z, sp.rangeMax.z - 0.01f);

        ImGui::SliderFloat("Radius", &sp.radius, 0.005f, 0.2f, "%.3f");

        struct MatOption { const char* name; uint id; float3 color; };
        static const MatOption opts[] = {
            { "Mirror",     MAT_MIRROR,     float3(0.9f, 0.9f, 0.95f) },
            { "Dielectric", MAT_DIELECTRIC, float3(0.8f, 0.9f, 1.0f)  },
            { "Green",      MAT_GREEN,      float3(0.2f, 0.8f, 0.3f)  },
        };
        constexpr int N = sizeof(opts) / sizeof(opts[0]);

        const auto& cur = opts[sp.matChoice];
        ImGui::ColorButton("##mc", ImVec4(cur.color.x, cur.color.y, cur.color.z, 1),
            ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker, ImVec2(14, 14));
        ImGui::SameLine();
        if (ImGui::BeginCombo("Material", cur.name))
        {
            for (int i = 0; i < N; i++)
            {
                ImGui::ColorButton("##o", ImVec4(opts[i].color.x, opts[i].color.y,
                    opts[i].color.z, 1),
                    ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker,
                    ImVec2(12, 12));
                ImGui::SameLine();
                if (ImGui::Selectable(opts[i].name, sp.matChoice == i))
                    sp.matChoice = i;
            }
            ImGui::EndCombo();
        }

        ImGui::Spacing();
        static const char* labels[] = { "1", "10", "100", "1000" };
        ImGui::Text("Count");
        for (int i = 0; i < SpawnerState::NUM_COUNTS; i++)
        {
            ImGui::SameLine();
            bool active = (sp.countChoice == i);
            if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            if (ImGui::Button(labels[i], ImVec2(40, 0))) sp.countChoice = i;
            if (active) ImGui::PopStyleColor();
        }

        ImGui::Spacing();
        float hw = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

        if (ImGui::Button("Spawn", ImVec2(hw, 0)))
        {
            int n = SpawnerState::countValues[sp.countChoice];
            uint matID = opts[sp.matChoice].id;
            scene.spheres.reserve(scene.spheres.size() + n);
            for (int i = 0; i < n; i++)
                scene.spheres.push_back(Sphere{
                    float3(sp.rangeMin.x + RandomFloat() * (sp.rangeMax.x - sp.rangeMin.x),
                           sp.rangeMin.y + RandomFloat() * (sp.rangeMax.y - sp.rangeMin.y),
                           sp.rangeMin.z + RandomFloat() * (sp.rangeMax.z - sp.rangeMin.z)),
                    sp.radius, matID });
            scene.BuildSphereBVH();
            if (resetAcc) resetAcc();
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear All", ImVec2(hw, 0)))
        {
            scene.spheres.clear();
            scene.BuildSphereBVH();
            if (resetAcc) resetAcc();
        }

        int cnt = (int)scene.spheres.size();
        if (cnt > 0 && ImGui::TreeNode("Spheres##list"))
        {
            ImGui::Text("%d sphere%s", cnt, cnt == 1 ? "" : "s");
            int del = -1, show = min(cnt, 200);
            if (cnt > show) ImGui::TextDisabled("(showing first %d of %d)", show, cnt);
            for (int i = 0; i < show; i++)
            {
                ImGui::PushID(i);
                auto& sph = scene.spheres[i];
                ImGui::Text("#%d", i); ImGui::SameLine(40);
                ImGui::TextDisabled("(%.2f,%.2f,%.2f) r=%.3f m=%u",
                    sph.center.x, sph.center.y, sph.center.z, sph.radius, sph.material);
                ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 20);
                if (ImGui::SmallButton("X")) del = i;
                ImGui::PopID();
            }
            if (del >= 0)
            {
                scene.spheres.erase(scene.spheres.begin() + del);
                scene.BuildSphereBVH();
                if (resetAcc) resetAcc();
            }
            ImGui::TreePop();
        }

        ImGui::Spacing();
        if (ImGui::Checkbox("Use BVH (legacy)", &scene.useLegacyBVH))
        {
            scene.BuildSphereBVH();
            if (resetAcc) resetAcc();
        }
        ImGui::SameLine();
        ImGui::TextDisabled(scene.useLegacyBVH ? "(SAH BVH2)" : "(uniform grid)");
    }

    // ─────────────────────────────────────────────
    // Physics demo
    // ─────────────────────────────────────────────

    inline SceneDef PhysicsDemo()
    {
        SceneDef s;
        s.name = "Physics";
        s.useVoxelGrid = true;
        s.usePhysics = true;

        s.camPos = float3(0.5f, 0.65f, -0.2f);
        s.camTarget = float3(0.5f, 0.25f, 0.5f);

        s.sky.sunDir = normalize(float3(0.4f, -0.7f, 0.3f));
        s.sky.sunColor = float3(1.0f, 0.95f, 0.8f);
        s.sky.sunIntensity = 2.5f;
        s.sky.timeOfDay = 0.25f;

        s.gridBuilder = [](Tmpl8::Scene& scene) {
            for (int x = 0; x < 512; x++)
                for (int z = 0; z < 512; z++)
                    scene.SetVoxel(x, 64, z, 200);
            };

        for (int i = 0; i < 12; i++)
        {
            float xOff = (i % 4) * 0.04f - 0.06f;
            float zOff = (i / 4) * 0.04f;
            uint mat = (i % 3 == 0) ? MAT_MIRROR : (i % 3 == 1) ? MAT_DIELECTRIC : MAT_GREEN;
            s.spheres.push_back({ float3(0.5f + xOff, 0.45f + 0.02f * i, 0.45f + zOff), 0.015f, mat });
        }

        s.spawner.radius = 0.015f;
        s.spawner.rangeMin = float3(0.35f, 0.50f, 0.40f);
        s.spawner.rangeMax = float3(0.65f, 0.55f, 0.50f);

        s.pointLights.push_back({ float3(0.5f, 0.85f, 0.5f), float3(1.5f, 1.45f, 1.4f), true });
        s.pointLights.push_back({ float3(0.5f, 0.4f, 0.1f),  float3(0.6f, 0.58f, 0.55f), true });

        s.uiCallback = [](SceneDef& def, Tmpl8::Scene& scene, std::function<void()> resetAcc) {
            SphereSpawnerUI(def, scene, resetAcc);
            };

        return s;
    }

    // ═════════════════════════════════════════════
    //  REGISTRATION
    // ═════════════════════════════════════════════

    inline void RegisterAllScenes(SceneManager& mgr)
    {
        mgr.AddScene(CubeShowcase());        // 0
        mgr.AddScene(MengerShowcase());      // 1
        mgr.AddScene(LivingCubeShowcase());  // 2
        mgr.AddScene(PulseGridShowcase());   // 3
        mgr.AddScene(HelixShowcase());       // 4
        mgr.AddScene(OrbitCloudShowcase());  // 5
        mgr.AddScene(BrickmapShowcase());    // 6
        mgr.AddScene(InfinityMirrorShowcase()); // 7
        mgr.AddScene(GyroscopeShowcase());   // 8
        mgr.AddScene(OutroShowcase());       // 9
        mgr.AddScene(PhysicsDemo());         // 10
    }

} // namespace GameScenes