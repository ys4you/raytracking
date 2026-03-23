#include "template.h"
#include "Core/ShadingPoint.h"
#include "Core/Lighting/PointLight.h"
#include "Core/Lighting/DirectionalLight.h"
#include "Core/Lighting/SpotLight.h"
#include "Core/Lighting/AreaLight.h"
#include <immintrin.h>


float3 Renderer::Trace(Ray& ray, int depth, int, int)
{
    constexpr int MAX_DEPTH = 5;
    if (depth >= MAX_DEPTH)
        return float3(0, 0, 0);
    if (depth >= 2)
    {
        constexpr float surviveP = 0.75f;
        if (RandomFloat() > surviveP)
            return float3(0, 0, 0);
    }

    scene.FindNearest(ray);

    // ── Miss check ────────────────────────────────────────────
    if (ray.voxel == 0 && ray.sphereIndex < 0 && ray.instanceIndex < 0)
        return sky.GetSkyColor(ray.D);

    // ── Material lookup ───────────────────────────────────────
    const Material* matPtr = nullptr;
    if (ray.sphereIndex >= 0)
        matPtr = &scene.GetSphereMat((uint)scene.sphereSOA.material[ray.sphereIndex]);
    else if (ray.instanceIndex >= 0 && ray.voxel > 0)
        matPtr = &scene.GetMat(ray.voxel);
    else if (ray.voxel > 0)
        matPtr = &scene.GetMat(ray.materialIndex);

    if (!matPtr) return sky.GetSkyColor(ray.D);
    const Material& mat = *matPtr;

    // ── Fast sphere shading (LOD optimisation) ────────────────
    if (fastSphereShading && ray.sphereIndex >= 0 &&
        static_cast<int>(scene.spheres.size()) >= fastSphereThreshold)
    {
        const float3 hitPos = ray.O + ray.t * ray.D;
        const float3 centre = float3(
            scene.sphereSOA.cx[ray.sphereIndex],
            scene.sphereSOA.cy[ray.sphereIndex],
            scene.sphereSOA.cz[ray.sphereIndex]
        );
        const float3 diff = hitPos - centre;
        const float  invLen = _mm_cvtss_f32(_mm_rsqrt_ss(_mm_set_ss(dot(diff, diff))));
        const float3 N = diff * invLen;
        const float  ndotl = max(0.0f, dot(N, fastSphereLightDir));
        return mat.albedo * (fastSphereAmbient + (1.0f - fastSphereAmbient) * ndotl);
    }

    // ── Shading point ─────────────────────────────────────────
    ShadingPoint sp;
    sp.position = ray.IntersectionPoint();
    sp.normal = ray.GetNormal(scene);
    sp.albedo = ray.GetAlbedo(scene);

    if (debugNormals)
        return 0.5f * (sp.normal + float3(1.0f));

    // ── Material shading ──────────────────────────────────────
    switch (mat.type)
    {
    case MaterialType::Lambertian:
    {
        float3 color = 0;
        for (const PointLight& l : lights.points)             if (l.enabled) color += IlluminatePoint(l, sp, scene);
        for (const DirectionalLight& l : lights.directionals) if (l.enabled) color += IlluminateDirectional(l, sp, scene);
        for (const SpotLight& l : lights.spots)               if (l.enabled) color += IlluminateSpot(l, sp, scene);
        for (const AreaLight& l : lights.areas)               if (l.enabled) color += IlluminateArea(l, sp, scene);
        color *= mat.albedo;
        return color;
    }
    case MaterialType::Metal:
    {
        float3 N = sp.normal;
        float3 R = normalize(ray.D - 2.0f * dot(ray.D, N) * N);
        if (mat.roughness > 0.0f) R += mat.roughness * RandomInUnitSphere();
        R = normalize(R);
        Ray reflectedRay(sp.position + N * EPSILON, R);
        return Trace(reflectedRay, depth + 1) * mat.albedo;
    }
    case MaterialType::Dielectric:
    {
        float3 N = sp.normal;
        float3 I = normalize(ray.D);
        float3 refracted;
        float  ni_over_nt = dot(I, N) > 0 ? mat.ior : 1.0f / mat.ior;
        float  reflect_prob = 1.0f;
        if (Refract(I, N, ni_over_nt, refracted))
            reflect_prob = Schlick(dot(I, N), mat.ior);
        if (RandomFloat() < reflect_prob)
        {
            Ray reflectedRay(sp.position + N * EPSILON, reflect(I, N));
            return Trace(reflectedRay, depth + 1);
        }
        else
        {
            Ray refractedRay(sp.position - N * EPSILON, refracted);
            return Trace(refractedRay, depth + 1);
        }
    }
    case MaterialType::Emissive:
        return mat.emission * mat.emissionStr;
    }

    return sky.GetSkyColor(ray.D);
}
// -----------------------------------------------------------
// Init
// -----------------------------------------------------------
void Renderer::Init()
{
    std::cout << "screen width: " << SCRWIDTH << " screen height: " << SCRHEIGHT << std::endl;

    sampleCountPerPixel = new int[SCRWIDTH * SCRHEIGHT]();

    Surface* bn = new Surface("assets/BlueNoise256x256.png");
    assert(bn->width == BN_SIZE && bn->height == BN_SIZE);
    blueNoise = new uint8_t[BN_SIZE * BN_SIZE];
    for (int i = 0; i < BN_SIZE * BN_SIZE; i++)
    {
        uint p = bn->pixels[i];
        blueNoise[i] = (uint8_t)((p >> 16) & 255);
    }
    delete bn;

    lights.directionals.push_back(sky.sun);
    lights.directionals.push_back(sky.moon);
    {
        PointLight pl{ { 1,1,1 }, { 1,1,1 }, false };
        lights.points.push_back(pl);
    }

    history = new float3[SCRWIDTH * SCRHEIGHT];
    memset(history, 0, SCRWIDTH * SCRHEIGHT * sizeof(float3));

    rayDirTable = new float3[SCRWIDTH * SCRHEIGHT];
    rayTableDirty = true;

    InitAccumulator();

    for (int i = 0; i < static_cast<int>(scene.spheres.size()); i++)
    {
        int b = physics.AddBall(scene.spheres[i].center, scene.spheres[i].radius);
        physics.balls[b].visualIndex = i;
    }

    const float3 orbitCenter = float3(0.5f, 0.5f, 0.5f);
    constexpr float radius = 2.f;
    cameraSpline.points =
    {
        orbitCenter + float3(radius, 0,      0),
        orbitCenter + float3(0,      0,  radius),
        orbitCenter + float3(-radius, 0,      0),
        orbitCenter + float3(0,      0, -radius),
        orbitCenter + float3(radius, 0,      0)
    };
    cameraSpline.BuildArcLengthTable();
    cameraFollower.spline = &cameraSpline;
    cameraFollower.speed = 0.2f;
    cameraFollower.loop = true;

    GameScenes::RegisterAllScenes(sceneManager);
    sceneManager.LoadScene(0, scene, camera, sky, lights);
}


// -----------------------------------------------------------
// Tick
// -----------------------------------------------------------
void Renderer::Tick(float deltaTime)
{
    const float3 orbitCenter = float3(0.5f, 0.5f, 0.5f);

    auto startTime = std::chrono::high_resolution_clock::now();
    sampleCount++;
    int totalRaysThisFrame = 0;

    sky.Update(deltaTime);

    float dt = deltaTime * 0.001f;

    if (useSplineCamera)
    {
        cameraFollower.Update(dt);
        camera.camPos = cameraFollower.position;
        camera.camTarget = orbitCenter;
    }

    if (lights.directionals.size() >= 2)
    {
        lights.directionals[0] = sky.sun;
        lights.directionals[1] = sky.moon;
    }

    const bool cameraMoving =
        length(camera.camPos - prevCamera.camPos) > 1e-4f ||
        length(camera.camTarget - prevCamera.camTarget) > 1e-4f;

    if (cameraMoving) rayTableDirty = true;

    if (!cameraMoving && rayTableDirty)
    {
#pragma omp parallel for schedule(static)
        for (int y = 0; y < SCRHEIGHT; y++)
            for (int x = 0; x < SCRWIDTH; x++)
                rayDirTable[x + y * SCRWIDTH] =
                camera.GetPrimaryRay(static_cast<float>(x) + 0.5f,
                    static_cast<float>(y) + 0.5f).D;
        rayTableDirty = false;
    }

    physics.Update(dt, scene);

    bool anyBallMoved = !physics.balls.empty();
    for (auto& ball : physics.balls)
        if (ball.visualIndex >= 0 && ball.visualIndex < static_cast<int>(scene.spheres.size()))
            scene.spheres[ball.visualIndex].center = ball.position;

    if (anyBallMoved)
    {
        scene.BuildSphereBVH();
        ResetAccumulator();
    }

    // ── Per-frame scene logic (animation, voxel re-stamping, etc.) ──
    // Runs BEFORE the OpenMP pixel loop — must not overlap with ray tracing.
    if (sceneManager.HasActive() && sceneManager.Active().tickCallback)
    {
        sceneManager.Active().tickCallback(
            sceneManager.Active(), scene, deltaTime,
            [this]() { ResetAccumulator(); }
        );
    }

    constexpr int TILE = 16;
    const int tilesX = (SCRWIDTH + TILE - 1) / TILE;
    const int tilesY = (SCRHEIGHT + TILE - 1) / TILE;
    const int totalTiles = tilesX * tilesY;

#pragma omp parallel for schedule(dynamic, 1) reduction(+:totalRaysThisFrame)
    for (int tileIdx = 0; tileIdx < totalTiles; ++tileIdx)
    {
        const int tileRow = tileIdx / tilesX;
        const int tileCol = tileIdx % tilesX;
        const int x0 = tileCol * TILE;
        const int y0 = tileRow * TILE;
        const int x1 = min(x0 + TILE, SCRWIDTH);
        const int y1 = min(y0 + TILE, SCRHEIGHT);

        for (int y = y0; y < y1; ++y)
            for (int x = x0; x < x1; ++x)
            {
                const int idx = x + y * SCRWIDTH;

                const float jx = BlueNoise(x, y, sampleCount);
                const float jy = BlueNoise(y, x, sampleCount);

                Ray r = (cameraMoving || rayTableDirty)
                    ? camera.GetPrimaryRay(x + jx, y + jy)
                    : Ray(camera.camPos, rayDirTable[idx]);

                const float3 sample = Trace(r, 0, 0, 0);
                totalRaysThisFrame++;

                float3 blended;

                if (!cameraMoving)
                {
                    blended = sample;
                    if (r.voxel != 0)
                        sampleCountPerPixel[idx] = 1;
                }
                else if (r.voxel > 0)
                {
                    const float3 P = r.O + r.t * r.D;
                    float prev_x, prev_y;

                    if (prevCamera.WorldToScreen(P, prev_x, prev_y))
                    {
                        const int   ix = static_cast<int>(prev_x);
                        const int   iy = static_cast<int>(prev_y);
                        const float fx = prev_x - (float)ix;
                        const float fy = prev_y - (float)iy;

                        if (ix < 0 || ix + 1 >= SCRWIDTH ||
                            iy < 0 || iy + 1 >= SCRHEIGHT)
                        {
                            blended = sample;
                            sampleCountPerPixel[idx] = 1;
                        }
                        else
                        {
                            const float3 h00 = history[ix + iy * SCRWIDTH];
                            const float3 h10 = history[ix + 1 + iy * SCRWIDTH];
                            const float3 h01 = history[ix + (iy + 1) * SCRWIDTH];
                            const float3 h11 = history[ix + 1 + (iy + 1) * SCRWIDTH];

                            const float3 top = lerp(h00, h10, fx);
                            const float3 bot = lerp(h01, h11, fx);
                            float3 historySample = lerp(top, bot, fy);

                            blended = 0.8f * historySample + 0.2f * sample;
                            sampleCountPerPixel[idx] = 6;
                        }
                    }
                    else
                    {
                        blended = sample;
                        sampleCountPerPixel[idx] = 1;
                    }
                }
                else
                {
                    blended = sample;
                    sampleCountPerPixel[idx] = 1;
                }

                accumulator[idx] = blended;
                screen->pixels[idx] = RGBF32_to_RGB8(blended);
            }
    }

    prevCamera = camera;
    camera.HandleInput(deltaTime);
    swap(history, accumulator);

    auto endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float> frameDuration = endTime - startTime;
    lastFrameTime = frameDuration.count();
    avgFrameTimeMs = lastFrameTime * 1000.0f;
    fps = 1.0f / lastFrameTime;
    rps = (float)totalRaysThisFrame / (lastFrameTime * 1'000'000.0f);

    if (rebuildSphereBVH)
    {
        scene.BuildSphereBVH();
        rebuildSphereBVH = false;
    }
}


// -----------------------------------------------------------
// Stats window — compact, always visible
// -----------------------------------------------------------
void Renderer::UIStats()
{
    ImGui::Begin("Stats", nullptr, ImGuiWindowFlags_NoCollapse);

    ImGui::Text("Renderer");
    ImGui::Separator();
    ImGui::Text("Voxel: %i",
        camera.GetPinholeRay(static_cast<float>(mousePos.x),
            static_cast<float>(mousePos.y)).voxel);
    ImGui::Text("%.2f ms | %.1f FPS", avgFrameTimeMs, fps);
    ImGui::Text("%.1f Mrays/s", rps);

    ImGui::Text("Ray table: %s", rayTableDirty ? "dirty" : "cached");

    static float fpsHistory[120] = {};
    static int   offset = 0;
    fpsHistory[offset] = fps;
    offset = (offset + 1) % IM_ARRAYSIZE(fpsHistory);

    ImGui::PlotLines("FPS", fpsHistory, IM_ARRAYSIZE(fpsHistory), offset,
        nullptr, 0.0f, 120.0f, ImVec2(0, 60));

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 graphPos = ImGui::GetItemRectMin();
    ImVec2 graphSize = ImGui::GetItemRectSize();
    float  y60 = graphPos.y + graphSize.y * (1.0f - 60.0f / 120.0f);
    draw_list->AddLine(ImVec2(graphPos.x, y60),
        ImVec2(graphPos.x + graphSize.x, y60),
        IM_COL32(255, 0, 0, 255));

    ImGui::End();
}


// -----------------------------------------------------------
// Helper: single light editor with colored type indicator
// -----------------------------------------------------------
static void LightColorDot(float3 c)
{
    ImGui::ColorButton("##dot", ImVec4(c.x, c.y, c.z, 1.0f),
        ImGuiColorEditFlags_NoTooltip |
        ImGuiColorEditFlags_NoPicker |
        ImGuiColorEditFlags_NoBorder,
        ImVec2(10, 10));
    ImGui::SameLine();
}


// -----------------------------------------------------------
// Inspector panel
// -----------------------------------------------------------
void Renderer::UI()
{
    ImGui::Begin("Inspector");

    // ── Scene Manager (always at top) ─────────────────────────────────
    sceneManager.UI(scene, camera, sky, lights, [this]() { ResetAccumulator(); });


    UIStats();

    // ── Debug / Camera ────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Camera & Debug"))
    {
        if (ImGui::Checkbox("Show Normals", &debugNormals))
            ResetAccumulator();

        if (ImGui::Checkbox("Use Spline Camera", &useSplineCamera))
            ResetAccumulator();

        ImGui::Spacing();
        ImGui::TextDisabled("Position:  %.3f  %.3f  %.3f",
            camera.camPos.x, camera.camPos.y, camera.camPos.z);
        ImGui::TextDisabled("Target:    %.3f  %.3f  %.3f",
            camera.camTarget.x, camera.camTarget.y, camera.camTarget.z);

        ImGui::Spacing();
        bool camChanged = false;

        if (ImGui::TreeNode("Depth of Field"))
        {
            camChanged |= ImGui::SliderFloat("Aperture", &camera.aperture, 0.0f, 0.5f, "%.4f");
            camChanged |= ImGui::SliderFloat("Blur Factor", &camera.blurFactor, 0.0f, 1.0f);
            camChanged |= ImGui::DragFloat2("Focus Range", &camera.focusRange.x, 0.01f, 0.0f, 100.0f, "%.2f");
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Projection"))
        {
            camChanged |= ImGui::Checkbox("Use Fisheye", &camera.useFisheye);
            camChanged |= ImGui::SliderFloat("HFOV (deg)", &camera.hfov, 30.0f, 160.0f, "%.1f");

            if (!camera.useFisheye)
            {
                camChanged |= ImGui::SliderFloat("Panini d", &camera.panini_d, 0.0f, 1.5f, "%.3f");
                camChanged |= ImGui::SliderFloat("Panini s", &camera.panini_s, 0.0f, 1.0f, "%.3f");
            }
            ImGui::TreePop();
        }

        if (camChanged)
        {
            rayTableDirty = true;
            ResetAccumulator();
        }
    }

    // ── Sky ──────────────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Sky"))
    {
        bool skyChanged = false;

        skyChanged |= ImGui::SliderFloat("Time of Day", &sky.timeOfDay, 0.0f, 1.0f, "%.3f");
        skyChanged |= ImGui::SliderFloat("Cycle Speed", &sky.cycleSpeed, 0.0f, 0.1f, "%.4f");
        skyChanged |= ImGui::Checkbox("Animate", &sky.animate);

        if (ImGui::TreeNode("Sun"))
        {
            skyChanged |= ImGui::ColorEdit3("Noon Color", &sky.sunNoonColor.x);
            skyChanged |= ImGui::ColorEdit3("Horizon Color", &sky.sunHorizonColor.x);
            skyChanged |= ImGui::SliderFloat("Intensity", &sky.sunIntensity, 0.0f, 10.0f);
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Moon"))
        {
            skyChanged |= ImGui::ColorEdit3("Color", &sky.moonColor.x);
            skyChanged |= ImGui::SliderFloat("Intensity", &sky.moonIntensity, 0.0f, 1.0f);
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Atmosphere"))
        {
            skyChanged |= ImGui::ColorEdit3("Zenith", &sky.zenithColor.x);
            skyChanged |= ImGui::ColorEdit3("Horizon", &sky.horizonColor.x);
            ImGui::TreePop();
        }

        if (skyChanged) ResetAccumulator();
    }

    // ── Lights ──────────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Lights"))
    {
        bool lightsChanged = false;
        int  uid = 0;

        // Summary line
        int totalLights = (int)(lights.points.size() + lights.directionals.size() +
            lights.spots.size() + lights.areas.size());
        ImGui::TextDisabled("%d light%s", totalLights, totalLights == 1 ? "" : "s");
        ImGui::Spacing();

        // ── Point lights ───────────────────────────────────────
        for (size_t i = 0; i < lights.points.size(); i++)
        {
            PointLight& pl = lights.points[i];
            ImGui::PushID(uid++);

            LightColorDot(pl.color);
            char label[64];
            snprintf(label, sizeof(label), "Point %d", (int)i);
            bool open = ImGui::TreeNode(label);

            // Enable toggle on the right
            ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 24);
            if (ImGui::Checkbox("##en", &pl.enabled)) lightsChanged = true;

            if (open)
            {
                ImGui::BeginDisabled(!pl.enabled);
                lightsChanged |= ImGui::DragFloat3("Position", &pl.position.x, 0.01f);
                lightsChanged |= ImGui::ColorEdit3("Color", &pl.color.x);
                ImGui::EndDisabled();
                ImGui::TreePop();
            }
            ImGui::PopID();
        }

        // ── Directional lights ─────────────────────────────────
        for (size_t i = 0; i < lights.directionals.size(); i++)
        {
            DirectionalLight& dl = lights.directionals[i];
            ImGui::PushID(uid++);

            LightColorDot(dl.color);
            char label[64];
            snprintf(label, sizeof(label), "Directional %d", (int)i);
            bool open = ImGui::TreeNode(label);

            ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 24);
            if (ImGui::Checkbox("##en", &dl.enabled)) lightsChanged = true;

            if (open)
            {
                ImGui::BeginDisabled(!dl.enabled);
                bool dirChanged = ImGui::DragFloat3("Direction", &dl.direction.x, 0.01f);
                if (dirChanged) dl.direction = normalize(dl.direction);
                lightsChanged |= dirChanged;
                lightsChanged |= ImGui::ColorEdit3("Color", &dl.color.x);
                ImGui::EndDisabled();
                ImGui::TreePop();
            }
            ImGui::PopID();
        }

        // ── Spot lights ────────────────────────────────────────
        for (size_t i = 0; i < lights.spots.size(); i++)
        {
            SpotLight& sl = lights.spots[i];
            ImGui::PushID(uid++);

            LightColorDot(sl.color);
            char label[64];
            snprintf(label, sizeof(label), "Spot %d", (int)i);
            bool open = ImGui::TreeNode(label);

            ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 24);
            if (ImGui::Checkbox("##en", &sl.enabled)) lightsChanged = true;

            if (open)
            {
                ImGui::BeginDisabled(!sl.enabled);
                bool slChanged = false;
                slChanged |= ImGui::DragFloat3("Position", &sl.position.x, 0.01f);
                slChanged |= ImGui::DragFloat3("Direction", &sl.direction.x, 0.01f);
                if (slChanged) sl.direction = normalize(sl.direction);
                slChanged |= ImGui::ColorEdit3("Color", &sl.color.x);
                slChanged |= ImGui::DragFloat("Range", &sl.range, 0.1f, 0.1f, 100.0f);
                slChanged |= ImGui::DragFloat("Angle", &sl.spotAngleDeg, 0.1f, 0.1f, 90.0f);
                slChanged |= ImGui::SliderFloat("Edge Softness", &sl.edgeRoughness, 0.0f, 1.0f);
                lightsChanged |= slChanged;
                ImGui::EndDisabled();
                ImGui::TreePop();
            }
            ImGui::PopID();
        }

        // ── Area lights ────────────────────────────────────────
        for (size_t i = 0; i < lights.areas.size(); i++)
        {
            AreaLight& al = lights.areas[i];
            ImGui::PushID(uid++);

            LightColorDot(al.color);
            char label[64];
            snprintf(label, sizeof(label), "Area %d", (int)i);
            bool open = ImGui::TreeNode(label);

            ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 24);
            if (ImGui::Checkbox("##en", &al.enabled)) lightsChanged = true;

            if (open)
            {
                ImGui::BeginDisabled(!al.enabled);
                lightsChanged |= ImGui::ColorEdit3("Color", &al.color.x);
                lightsChanged |= ImGui::DragFloat3("Corner", &al.corner.x, 0.01f);
                lightsChanged |= ImGui::DragFloat3("Edge 1", &al.edge1.x, 0.01f);
                lightsChanged |= ImGui::DragFloat3("Edge 2", &al.edge2.x, 0.01f);
                ImGui::EndDisabled();
                ImGui::TreePop();
            }
            ImGui::PopID();
        }

        if (lightsChanged) ResetAccumulator();
    }

    // ── Materials ────────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Materials"))
    {
        bool materialsChanged = false;

        if (selectionLocked && selectedMaterialIndex != -1)
        {
            if (MaterialUI("Selected Material", scene.materials[selectedMaterialIndex]))
                materialsChanged = true;
        }
        else
        {
            ImGui::TextDisabled("Left-click a surface to select its material.");
            ImGui::TextDisabled("Right-click to deselect.");
        }

        if (materialsChanged) ResetAccumulator();
    }

    ImGui::End();
}


// -----------------------------------------------------------
// Material editor
// -----------------------------------------------------------
bool Renderer::MaterialUI(const char* label, Material& material)
{
    ImGui::PushID(label);
    bool changed = false;

    // Albedo preview dot next to the label
    ImGui::ColorButton("##alb", ImVec4(material.albedo.x, material.albedo.y,
        material.albedo.z, 1.0f),
        ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker,
        ImVec2(12, 12));
    ImGui::SameLine();

    if (ImGui::TreeNode(label))
    {
        static const char* TypeLabels[] = { "Lambertian", "Metal", "Dielectric", "Emissive" };
        int type = static_cast<int>(material.type);
        if (ImGui::Combo("Shader", &type, TypeLabels, IM_ARRAYSIZE(TypeLabels)))
        {
            material.type = static_cast<MaterialType>(type);
            changed = true;
        }

        changed |= ImGui::ColorEdit3("Albedo", &material.albedo.x);

        switch (material.type)
        {
        case MaterialType::Lambertian:
            changed |= ImGui::SliderFloat("Roughness", &material.roughness, 0.0f, 1.0f);
            break;
        case MaterialType::Metal:
            changed |= ImGui::SliderFloat("Roughness", &material.roughness, 0.0f, 1.0f);
            changed |= ImGui::SliderFloat("Metallic", &material.metallic, 0.0f, 1.0f);
            break;
        case MaterialType::Dielectric:
            changed |= ImGui::SliderFloat("IOR", &material.ior, 1.0f, 2.5f);

            // Quick presets
            if (ImGui::SmallButton("Glass")) { material.ior = 1.52f; changed = true; }
            ImGui::SameLine();
            if (ImGui::SmallButton("Water")) { material.ior = 1.33f; changed = true; }
            ImGui::SameLine();
            if (ImGui::SmallButton("Diamond")) { material.ior = 2.42f; changed = true; }
            break;
        case MaterialType::Emissive:
            changed |= ImGui::ColorEdit3("Emission", &material.emission.x);
            changed |= ImGui::SliderFloat("Intensity", &material.emissionStr, 0.0f, 50.0f);
            break;
        }

        ImGui::TreePop();
    }

    ImGui::PopID();
    return changed;
}


// -----------------------------------------------------------
void Tmpl8::Renderer::InitAccumulator()
{
    if (!accumulator)
        accumulator = static_cast<float3*>MALLOC64(SCRWIDTH * SCRHEIGHT * sizeof(float3));
    ResetAccumulator();
}

// -----------------------------------------------------------
void Tmpl8::Renderer::ResetAccumulator()
{
    memset(accumulator, 0, SCRWIDTH * SCRHEIGHT * sizeof(float3));
    memset(sampleCountPerPixel, 0, SCRWIDTH * SCRHEIGHT * sizeof(int));
    sampleCount = 0;
}

// -----------------------------------------------------------
void Renderer::MouseDown(int button)
{
    if (button == 0)
    {
        if (!selectionLocked)
        {
            Ray r = camera.GetPinholeRay(static_cast<float>(mousePos.x),
                static_cast<float>(mousePos.y));
            scene.FindNearest(r);
            if (r.materialIndex != -1)
            {
                selectedMaterialIndex = r.materialIndex;
                selectionLocked = true;
            }
        }
    }
    else if (button == 1)
    {
        selectedMaterialIndex = -1;
        selectionLocked = false;
    }
}

void Renderer::KeyDown(int key)
{
    if (key == GLFW_KEY_F1) { sceneManager.LoadScene(0, scene, camera, sky, lights); ResetAccumulator(); rayTableDirty = true; }
    if (key == GLFW_KEY_F2) { sceneManager.LoadScene(1, scene, camera, sky, lights); ResetAccumulator(); rayTableDirty = true; }
}