#include "template.h"
#include "Core/ShadingPoint.h"
#include "Core/Lighting/PointLight.h"
#include "Core/Lighting/DirectionalLight.h"
#include "Core/Lighting/SpotLight.h"
#include "Core/Lighting/AreaLight.h"
#include <immintrin.h> // _mm_rsqrt_ss for fast sphere normal


// -----------------------------------------------------------
/// @brief  Traces a ray through the scene and returns the
///         outgoing radiance (RGB) at the first intersection.
///
/// Handles both voxel and sphere geometry, dispatches to the
/// correct material shading branch, and recurses for
/// reflective / refractive materials up to MAX_DEPTH bounces.
///
/// @param ray   Primary or secondary ray (modified in-place by FindNearest).
/// @param depth Current recursion depth; terminates at MAX_DEPTH.
/// @return      Linear RGB colour for this ray path.
// -----------------------------------------------------------
float3 Renderer::Trace(Ray& ray, int depth, int, int)
{
    constexpr int MAX_DEPTH = 5;

    if (depth >= MAX_DEPTH) 
        return float3(0, 0, 0);

    // Russian roulette: probabilistically terminate secondary rays at depth >= 2.
    // This is unbiased — terminated paths are compensated by the 1/p weight on
    // surviving paths.  Saves ~15-25% of secondary ray cost without bias.
    if (depth >= 2)
    {
        constexpr float surviveP = 0.75f;
        if (RandomFloat() > surviveP) 
            return float3(0, 0, 0);
        // Surviving rays are NOT upweighted here because we return colour
        // directly (not path-traced energy); this is a simple termination
        // approximation suitable for Whitted-style tracing.
    }

    scene.FindNearest(ray);

    if (ray.voxel == 0 && ray.sphereIndex < 0)
        return sky.GetSkyColor(ray.D);

    const Material* matPtr = nullptr;

    if (ray.sphereIndex >= 0)
        matPtr = &scene.GetSphereMat((uint)scene.sphereSOA.material[ray.sphereIndex]);
    else if (ray.voxel > 0)
        matPtr = &scene.GetMat(ray.materialIndex);

    if (!matPtr) return float3(1, 0, 1);
    const Material& mat = *matPtr;

    // Fast-path for large sphere counts: skip normal/albedo/position computation
    // entirely and return a cheap directional shading term. Moved before GetNormal
    // and GetAlbedo to avoid their cost (~sqrt + SOA reads) on every sphere hit.
    if (fastSphereShading && ray.sphereIndex >= 0 &&
        static_cast<int>(scene.spheres.size()) >= fastSphereThreshold)
    {
        // Reconstruct normal inline — avoids full GetNormal call overhead.
        const float3 hitPos = ray.O + ray.t * ray.D;
        const float3 centre = float3(
            scene.sphereSOA.cx[ray.sphereIndex],
            scene.sphereSOA.cy[ray.sphereIndex],
            scene.sphereSOA.cz[ray.sphereIndex]
        );
        const float3 diff = hitPos - centre;
        // rsqrtf is a single SSE instruction — faster than normalize()'s sqrtf.
        const float  invLen = _mm_cvtss_f32(_mm_rsqrt_ss(_mm_set_ss(dot(diff, diff))));
        const float3 N = diff * invLen;
        const float  ndotl = max(0.0f, dot(N, fastSphereLightDir));
        return mat.albedo * (fastSphereAmbient + (1.0f - fastSphereAmbient) * ndotl);
    }

    ShadingPoint sp;
    sp.position = ray.IntersectionPoint();
    sp.normal = ray.GetNormal(scene);
    sp.albedo = ray.GetAlbedo(scene);

    if (debugNormals)
        return 0.5f * (sp.normal + float3(1.0f));

    switch (mat.type)
    {
    case MaterialType::Lambertian:
    {
        float3 color = 0;
        for (const PointLight& l : lights.points)      if (l.enabled) color += IlluminatePoint(l, sp, scene);
        for (const DirectionalLight& l : lights.directionals) if (l.enabled) color += IlluminateDirectional(l, sp, scene);
        for (const SpotLight& l : lights.spots)        if (l.enabled) color += IlluminateSpot(l, sp, scene);
        for (const AreaLight& l : lights.areas)        if (l.enabled) color += IlluminateArea(l, sp, scene);
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

    return float3(1, 0, 1);
}


// -----------------------------------------------------------
/// @brief  One-time initialisation — allocates all buffers and
///         builds the light list.
// -----------------------------------------------------------
void Renderer::Init()
{
    std::cout << "screen width: " << SCRWIDTH << " screen height: " << SCRHEIGHT << std::endl;

    sampleCountPerPixel = new int[SCRWIDTH * SCRHEIGHT]();

    // Blue-noise texture (single-channel, red only).
    Surface* bn = new Surface("assets/BlueNoise256x256.png");
    assert(bn->width == BN_SIZE && bn->height == BN_SIZE);
    blueNoise = new uint8_t[BN_SIZE * BN_SIZE];
    for (int i = 0; i < BN_SIZE * BN_SIZE; i++)
    {
        uint p = bn->pixels[i];
        blueNoise[i] = (uint8_t)((p >> 16) & 255);
    }
    delete bn;

    // Lights.
    lights.directionals.push_back(sky.sun);
    lights.directionals.push_back(sky.moon);
    {
        PointLight pl{ { 1,1,1 }, { 1,1,1 }, false };
        lights.points.push_back(pl);
    }

    // Frame buffers.
    history = new float3[SCRWIDTH * SCRHEIGHT];
    memset(history, 0, SCRWIDTH * SCRHEIGHT * sizeof(float3));

    // -----------------------------------------------------------------------
    // Ray direction table.
    //
    // Heap-allocated because SCRWIDTH*SCRHEIGHT*12 bytes (~5 MB at 800×600)
    // would overflow the default 1 MB Windows stack.
    //
    // The table is filled lazily: rayTableDirty starts true, so it is built
    // on the first stationary frame in Tick().
    // -----------------------------------------------------------------------
    rayDirTable = new float3[SCRWIDTH * SCRHEIGHT];
    rayTableDirty = true;

    InitAccumulator();

    for (int i = 0; i < static_cast<int>(scene.spheres.size()); i++)
    {
        int b = physics.AddBall(scene.spheres[i].center, scene.spheres[i].radius);
        physics.balls[b].visualIndex = i;
    }

    // Camera spline (demo / attract mode).
    const float3 orbitCenter = float3(0.5f, 0.5f, 0.5f);
    constexpr float radius = 2.f;
    cameraSpline.points =
    {
        orbitCenter + float3(radius, 0,      0),
        orbitCenter + float3(0,      0,  radius),
        orbitCenter + float3(-radius, 0,      0),
        orbitCenter + float3(0,      0, -radius),
        orbitCenter + float3(radius, 0,      0)  // repeat first for smooth loop
    };
    cameraSpline.BuildArcLengthTable();
    cameraFollower.spline = &cameraSpline;
    cameraFollower.speed = 0.2f;
    cameraFollower.loop = true;
}


// -----------------------------------------------------------
/// @brief  Per-frame update: fire primary rays, shade, accumulate,
///         and write to the screen buffer.
///
/// Three optimisations are active here:
///
///   1. Two-level DDA  (inside Scene::TraverseDDA — see scene.cpp).
///      Skips empty 8³ bricks in one coarse step rather than 8 fine
///      steps, dramatically reducing marching cost for sparse scenes.
///
///   2. Shadow rays skip the sphere BVH (inside Scene::IsOccluded).
///      Voxel occlusion only — spheres never block directional lights
///      in a meaningful way for this scene scale.
///
///   3. Ray direction table.
///      GetPrimaryRay() calls PaniniBaseDir() which uses atan/tan per
///      pixel — ~16% of frame time at 800×600.  When the camera is
///      stationary the directions are precomputed once (parallel) and
///      reused every frame until the camera moves again.
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

    // ── Detect camera movement ──────────────────────────────────────────────
    const bool cameraMoving =
        length(camera.camPos - prevCamera.camPos) > 1e-4f ||
        length(camera.camTarget - prevCamera.camTarget) > 1e-4f;

    // Mark the table dirty whenever the camera is moving so it is rebuilt
    // on the first stationary frame.
    if (cameraMoving) rayTableDirty = true;

    if (!cameraMoving && rayTableDirty)
    {
#pragma omp parallel for schedule(static)
        for (int y = 0; y < SCRHEIGHT; y++)
            for (int x = 0; x < SCRWIDTH; x++)
                rayDirTable[x + y * SCRWIDTH] =
                camera.GetPrimaryRay(static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f).D;
        rayTableDirty = false;
    }

    // ── Physics ──────────────────────────────────────────────────────────────
    physics.Update(dt, scene);

    // Sync physics ball positions → visual spheres
    bool anyBallMoved = !physics.balls.empty();
    for (auto& ball : physics.balls)
        if (ball.visualIndex >= 0 && ball.visualIndex < static_cast<int>(scene.spheres.size()))
            scene.spheres[ball.visualIndex].center = ball.position;

    // Rebuild BVH so rendering sees the new positions
    if (anyBallMoved)
    {
        scene.BuildSphereBVH();
        ResetAccumulator();
    }

    // 16×16 tiles improve cache reuse: adjacent pixels in a tile share BVH
    // traversal state and read nearby memory.  OpenMP distributes whole tiles
    // across threads so each thread works a contiguous region of the screen.
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

                // ── OPT 3 (continued): primary ray generation ──────────────────
                //
                // Moving camera: generate the full ray including jitter every
                // frame — TAA reprojection needs accurate per-frame directions.
                //
                // Stationary camera: reuse the cached direction from rayDirTable
                // and apply only a small blue-noise jitter on top.  This replaces
                // one GetPrimaryRay() call (which internally calls PaniniBaseDir()
                // with atan/tan) with a table lookup + one GetPrimaryRay() call
                // at a jittered pixel centre.
                const float jx = BlueNoise(x, y, sampleCount);
                const float jy = BlueNoise(y, x, sampleCount);

                // Use table when stationary and ready, otherwise full ray generation.
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
                // ── Moving camera — geometry hit: reproject from history ───────
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
                // ── Moving camera — sky hit: no accumulation ───────────────────
                else
                {
                    blended = sample;
                    sampleCountPerPixel[idx] = 1;
                }

                accumulator[idx] = blended;
                screen->pixels[idx] = RGBF32_to_RGB8(blended);

            } // end pixel body (x,y inner)
    } // end tile loop

    prevCamera = camera;
    camera.HandleInput(deltaTime);
    swap(history, accumulator);

    // Performance counters.
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
// @brief  Dedicated statistics window.
// -----------------------------------------------------------
void Renderer::UIStats()
{
    ImGui::Begin("Stats");

    ImGui::Text("Renderer");
    ImGui::Separator();
    ImGui::Text("Voxel: %i",
        camera.GetPinholeRay(static_cast<float>(mousePos.x),
            static_cast<float>(mousePos.y)).voxel);
    ImGui::Text("%.2f ms | %.1f FPS", avgFrameTimeMs, fps);
    ImGui::Text("%.1f Mrays/s", rps);

    // Ray table status — useful for debugging the dirty-flag logic.
    ImGui::Text("Ray table: %s", rayTableDirty ? "dirty" : "cached");

    static float fpsHistory[120] = {};
    static int offset = 0;
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
/// @brief  Inspector panel — camera, sky, lights, materials,
///         sphere spawner.
// -----------------------------------------------------------
void Renderer::UI()
{
    ImGui::Begin("Inspector");

    UIStats();

    ImGui::Spacing();

    // ===== DEBUG =====
    if (ImGui::CollapsingHeader("Debug", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Indent();
        if (ImGui::Checkbox("Show Normals", &debugNormals))
            ResetAccumulator();
        ImGui::Unindent();

        if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent();
            bool cameraChanged = false;

            if (ImGui::Checkbox("Use Spline Camera", &useSplineCamera))
                ResetAccumulator();

            ImGui::Text("Position:  %.3f  %.3f  %.3f",
                camera.camPos.x, camera.camPos.y, camera.camPos.z);
            ImGui::Text("Target:    %.3f  %.3f  %.3f",
                camera.camTarget.x, camera.camTarget.y, camera.camTarget.z);

            ImGui::Text("Depth of Field");
            cameraChanged |= ImGui::SliderFloat("Aperture", &camera.aperture, 0.0f, 0.5f, "%.4f");
            cameraChanged |= ImGui::SliderFloat("Blur Factor", &camera.blurFactor, 0.0f, 1.0f);
            cameraChanged |= ImGui::DragFloat2("Focus Range", &camera.focusRange.x, 0.01f, 0.0f, 100.0f, "%.2f");

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            cameraChanged |= ImGui::Checkbox("Use Fisheye", &camera.useFisheye);
            ImGui::Text("Projection");
            cameraChanged |= ImGui::SliderFloat("HFOV (deg)", &camera.hfov, 30.0f, 160.0f, "%.1f");

            if (!camera.useFisheye)
            {
                cameraChanged |= ImGui::SliderFloat("Panini d", &camera.panini_d, 0.0f, 1.5f, "%.3f");
                cameraChanged |= ImGui::SliderFloat("Panini s", &camera.panini_s, 0.0f, 1.0f, "%.3f");
            }

            // Any camera parameter change invalidates the ray direction table
            // as well as the accumulator.
            if (cameraChanged)
            {
                rayTableDirty = true;
                ResetAccumulator();
            }

            ImGui::Unindent();
        }
    }

    ImGui::Spacing();

    // ===== SKY =====
    if (ImGui::CollapsingHeader("Sky", ImGuiTreeNodeFlags_DefaultOpen))
    {
        bool skyChanged = false;

        skyChanged |= ImGui::SliderFloat("Time of Day", &sky.timeOfDay, 0.0f, 1.0f, "%.3f");
        skyChanged |= ImGui::SliderFloat("Cycle Speed", &sky.cycleSpeed, 0.0f, 0.1f, "%.4f");
        skyChanged |= ImGui::Checkbox("Animate", &sky.animate);

        ImGui::Spacing(); ImGui::Separator(); ImGui::Text("Sun");
        skyChanged |= ImGui::ColorEdit3("Sun Color (noon)", &sky.sunNoonColor.x);
        skyChanged |= ImGui::ColorEdit3("Sun Color (horizon)", &sky.sunHorizonColor.x);
        skyChanged |= ImGui::SliderFloat("Sun Intensity", &sky.sunIntensity, 0.0f, 10.0f);

        ImGui::Spacing(); ImGui::Separator(); ImGui::Text("Moon");
        skyChanged |= ImGui::ColorEdit3("Moon Color", &sky.moonColor.x);
        skyChanged |= ImGui::SliderFloat("Moon Intensity", &sky.moonIntensity, 0.0f, 1.0f);

        ImGui::Spacing(); ImGui::Separator(); ImGui::Text("Sky Colors");
        skyChanged |= ImGui::ColorEdit3("Zenith", &sky.zenithColor.x);
        skyChanged |= ImGui::ColorEdit3("Horizon", &sky.horizonColor.x);

        if (skyChanged) ResetAccumulator();
    }

    ImGui::Spacing();

    // ===== LIGHTS =====
    if (ImGui::CollapsingHeader("Lights", ImGuiTreeNodeFlags_DefaultOpen))
    {
        bool lightsChanged = false;
        int  index = 0;

        for (size_t i = 0; i < lights.points.size(); i++)
        {
            PointLight& pl = lights.points[i];
            ImGui::PushID(index++);
            bool open = ImGui::CollapsingHeader("##lightHeader",
                pl.enabled ? ImGuiTreeNodeFlags_DefaultOpen : 0);
            ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 24);
            if (ImGui::Checkbox("##enabled", &pl.enabled)) lightsChanged = true;
            ImGui::SameLine(30);
            ImGui::TextUnformatted("Point Light");
            if (open)
            {
                ImGui::Indent();
                ImGui::BeginDisabled(!pl.enabled);
                lightsChanged |= ImGui::DragFloat3("Position", &pl.position.x, 0.1f);
                lightsChanged |= ImGui::ColorEdit3("Color", &pl.color.x);
                ImGui::EndDisabled();
                ImGui::Unindent();
            }
            ImGui::PopID();
        }

        for (size_t i = 0; i < lights.directionals.size(); i++)
        {
            DirectionalLight& dl = lights.directionals[i];
            ImGui::PushID(index++);
            bool open = ImGui::CollapsingHeader("##lightHeader",
                dl.enabled ? ImGuiTreeNodeFlags_DefaultOpen : 0);
            ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 24);
            if (ImGui::Checkbox("##enabled", &dl.enabled)) lightsChanged = true;
            ImGui::SameLine(30);
            ImGui::TextUnformatted("Directional Light");
            if (open)
            {
                ImGui::Indent();
                ImGui::BeginDisabled(!dl.enabled);
                bool dirChanged = ImGui::DragFloat3("Direction", &dl.direction.x, 0.01f);
                if (dirChanged) dl.direction = normalize(dl.direction);
                lightsChanged |= dirChanged;
                lightsChanged |= ImGui::ColorEdit3("Color", &dl.color.x);
                ImGui::EndDisabled();
                ImGui::Unindent();
            }
            ImGui::PopID();
        }

        for (size_t i = 0; i < lights.spots.size(); i++)
        {
            SpotLight& sl = lights.spots[i];
            ImGui::PushID(index++);
            bool open = ImGui::CollapsingHeader("##lightHeader",
                sl.enabled ? ImGuiTreeNodeFlags_DefaultOpen : 0);
            ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 24);
            if (ImGui::Checkbox("##enabled", &sl.enabled)) lightsChanged = true;
            ImGui::SameLine(30);
            ImGui::TextUnformatted("Spot Light");
            if (open)
            {
                ImGui::Indent();
                ImGui::BeginDisabled(!sl.enabled);
                bool slChanged = false;
                slChanged |= ImGui::DragFloat3("Position", &sl.position.x, 0.1f);
                slChanged |= ImGui::DragFloat3("Direction", &sl.direction.x, 0.01f);
                if (slChanged) sl.direction = normalize(sl.direction);
                slChanged |= ImGui::ColorEdit3("Color", &sl.color.x);
                slChanged |= ImGui::DragFloat("Range", &sl.range, 0.1f, 0.1f, 100.0f);
                slChanged |= ImGui::DragFloat("Angle", &sl.spotAngleDeg, 0.1f, 0.1f, 90.0f);
                slChanged |= ImGui::SliderFloat("Edge Softness", &sl.edgeRoughness, 0.0f, 1.0f);
                lightsChanged |= slChanged;
                ImGui::EndDisabled();
                ImGui::Unindent();
            }
            ImGui::PopID();
        }

        for (size_t i = 0; i < lights.areas.size(); i++)
        {
            AreaLight& al = lights.areas[i];
            ImGui::PushID(index++);
            bool open = ImGui::CollapsingHeader("##lightHeader",
                al.enabled ? ImGuiTreeNodeFlags_DefaultOpen : 0);
            ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 24);
            if (ImGui::Checkbox("##enabled", &al.enabled)) lightsChanged = true;
            ImGui::SameLine(30);
            ImGui::TextUnformatted("Area Light");
            if (open)
            {
                ImGui::Indent();
                ImGui::BeginDisabled(!al.enabled);
                lightsChanged |= ImGui::ColorEdit3("Color", &al.color.x);
                lightsChanged |= ImGui::DragFloat3("Corner", &al.corner.x, 0.1f);
                lightsChanged |= ImGui::DragFloat3("Edge 1", &al.edge1.x, 0.1f);
                lightsChanged |= ImGui::DragFloat3("Edge 2", &al.edge2.x, 0.1f);
                ImGui::EndDisabled();
                ImGui::Unindent();
            }
            ImGui::PopID();
        }

        if (lightsChanged) ResetAccumulator();
    }

    ImGui::Spacing();

    // ===== MATERIALS =====
    if (ImGui::CollapsingHeader("Materials", ImGuiTreeNodeFlags_DefaultOpen))
    {
        bool materialsChanged = false;

        if (selectionLocked && selectedMaterialIndex != -1)
        {
            if (MaterialUI("Selected Material", scene.materials[selectedMaterialIndex]))
                materialsChanged = true;
            ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        }

        ImGui::Text("Global Materials");
        ImGui::Spacing();
        if (MaterialUI("Mirror", scene.materials[MAT_MIRROR]))     materialsChanged = true;
        if (MaterialUI("Dielectric", scene.materials[MAT_DIELECTRIC])) materialsChanged = true;

        if (materialsChanged) ResetAccumulator();
    }

    // ===== SPHERE SPAWNER =====
    if (ImGui::CollapsingHeader("Sphere Spawner", ImGuiTreeNodeFlags_DefaultOpen))
    {
        static float3 spawnMin = { 0.1f, 0.1f, 0.1f };
        static float3 spawnMax = { 0.9f, 0.9f, 0.9f };
        static float  spawnRadius = 0.05f;
        static int    spawnMatIndex = MAT_MIRROR;

        static const char* countLabels[] = { "1", "10", "100", "1000" };
        static constexpr int   countValues[] = { 1,   10,   100,   1000 };
        static int selectedCount = 0;

        ImGui::Text("Spawn Range");
        ImGui::DragFloat3("Min XYZ", &spawnMin.x, 0.01f, 0.0f, 1.0f, "%.2f");
        ImGui::DragFloat3("Max XYZ", &spawnMax.x, 0.01f, 0.0f, 1.0f, "%.2f");

        spawnMin.x = min(spawnMin.x, spawnMax.x - 0.01f);
        spawnMin.y = min(spawnMin.y, spawnMax.y - 0.01f);
        spawnMin.z = min(spawnMin.z, spawnMax.z - 0.01f);

        ImGui::Spacing();
        ImGui::SliderFloat("Radius", &spawnRadius, 0.005f, 0.2f, "%.3f");

        static const char* matNames[] = { "Mirror", "Dielectric", "Green" };
        static constexpr uint  matIndices[] = { (uint)MAT_MIRROR, (uint)MAT_DIELECTRIC, (uint)MAT_GREEN };
        static int selectedMat = 0;
        ImGui::Combo("Material", &selectedMat, matNames, IM_ARRAYSIZE(matNames));
        spawnMatIndex = matIndices[selectedMat];

        ImGui::Spacing();
        ImGui::Text("Count");
        ImGui::SameLine();

        for (int i = 0; i < 4; i++)
        {
            if (i > 0) ImGui::SameLine();
            bool active = (selectedCount == i);
            if (active) ImGui::PushStyleColor(ImGuiCol_Button,
                ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            if (ImGui::Button(countLabels[i], ImVec2(48, 0)))
                selectedCount = i;
            if (active) ImGui::PopStyleColor();
        }

        ImGui::Spacing();
        if (ImGui::Button("Spawn Spheres", ImVec2(-1, 0)))
        {
            int n = countValues[selectedCount];
            scene.spheres.reserve(scene.spheres.size() + n);
            for (int i = 0; i < n; i++)
            {
                scene.spheres.push_back(Sphere{
                    float3(
                        spawnMin.x + RandomFloat() * (spawnMax.x - spawnMin.x),
                        spawnMin.y + RandomFloat() * (spawnMax.y - spawnMin.y),
                        spawnMin.z + RandomFloat() * (spawnMax.z - spawnMin.z)
                    ),
                    spawnRadius,
                    MAT_MIRROR
                    });
            }
            scene.BuildSphereBVH();
            ResetAccumulator();
        }

        ImGui::SameLine();
        if (ImGui::Button("Clear Spheres", ImVec2(-1, 0)))
        {
            scene.spheres.clear();
            scene.BuildSphereBVH();
            ResetAccumulator();
        }

        if (ImGui::Checkbox("Use BVH (legacy)", &scene.useLegacyBVH))
            ResetAccumulator();
        ImGui::SameLine();
        ImGui::TextDisabled(scene.useLegacyBVH ? "(SAH BVH2)" : "(uniform grid)");
        ImGui::Spacing();
    }

    ImGui::End();
}


// -----------------------------------------------------------
/// @brief  ImGui editor for a single material.
// -----------------------------------------------------------
bool Renderer::MaterialUI(const char* label, Material& material)
{
    ImGui::PushID(label);
    bool changed = false;

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
            break;
        case MaterialType::Emissive:
            changed |= ImGui::ColorEdit3("Emission Color", &material.emission.x);
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