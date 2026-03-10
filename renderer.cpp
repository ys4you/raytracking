#include "template.h"
#include "Core/ShadingPoint.h"
#include "Core/Lighting/Light.h"
#include "Core/Lighting/PointLight.h"
#include "Core/Lighting/DirectionalLight.h"
#include "Core/Lighting/SpotLight.h"
#include "Core/Lighting/AreaLight.h"


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
	constexpr int MAX_DEPTH = 5; // maximum number of recursive ray bounces

    // Abort recursion once the bounce limit is reached
    if (depth >= MAX_DEPTH) return float3(0, 0, 0);

    // Find the closest intersection (result stored inside ray)
    scene.FindNearest(ray);

    // If nothing was hit, return the sky colour
    if (ray.voxel == 0 && ray.sphereIndex < 0)
        return sky.GetSkyColor(ray.D);

    // Resolve which material pointer to use
    const Material* matPtr = nullptr;

    if (ray.sphereIndex >= 0)
        // Sphere hit — look up the sphere's material
        matPtr = &scene.GetSphereMat(scene.spheres[ray.sphereIndex].material);
    else if (ray.voxel > 0)
        // Voxel hit — use the material index stored on the ray
        matPtr = &scene.GetMat(ray.materialIndex);

    // Safety fallback: magenta indicates a missing material
    if (!matPtr) return float3(1, 0, 1);
    const Material& mat = *matPtr;

    // -------------------------
    // Build the shading point (world-space surface data)
    // -------------------------
    ShadingPoint sp;
    sp.position = ray.IntersectionPoint(); // world-space hit position
    sp.normal = ray.GetNormal(scene);    // outward-facing surface normal
    sp.albedo = ray.GetAlbedo(scene);    // base colour at the hit

    // Debug visualisation: map normals to [0,1] RGB and return early
    if (debugNormals)
        return 0.5f * (sp.normal + float3(1.0f));

    // Material shading — dispatch by material type
    switch (mat.type)
    {
    case MaterialType::Lambertian:
    {
        float3 color(0);

        // Accumulate contribution from every enabled light
        for (Light* light : lights)
            if (light->enabled)
                color += light->Illuminate(sp, scene) * mat.albedo;

        return color;
    }

    case MaterialType::Metal:
    {
        float3 N = sp.normal;

        // Perfect mirror reflection direction
        float3 R = normalize(ray.D - 2.0f * dot(ray.D, N) * N);

        // Optional roughness: perturb the reflection direction by a random offset
        if (mat.roughness > 0.0f)
            R += mat.roughness * RandomInUnitSphere();
        R = normalize(R);

        // Spawn the reflected ray slightly above the surface to avoid self-intersection
        Ray reflectedRay(sp.position + N * EPSILON, R);
        return Trace(reflectedRay, depth + 1) * mat.albedo;
    }

    case MaterialType::Dielectric:
    {
        float3 N = sp.normal;
        float3 I = normalize(ray.D); // normalised incident direction
        float3 refracted;

        // Determine whether the ray is entering or exiting the medium
        float ni_over_nt = dot(I, N) > 0 ? mat.ior : 1.0f / mat.ior;

        float reflect_prob = 1.0f; // default: total internal reflection

        // Attempt to compute the refraction direction; on success, use Schlick to
        // blend between reflection and refraction
        if (Refract(I, N, ni_over_nt, refracted))
            reflect_prob = Schlick(dot(I, N), mat.ior);

        if (RandomFloat() < reflect_prob)
        {
            // Reflect off the surface
            Ray reflectedRay(sp.position + N * EPSILON, reflect(I, N));
            return Trace(reflectedRay, depth + 1);
        }
        else
        {
            // Refract through the surface (offset origin inward to leave the surface)
            Ray refractedRay(sp.position - N * EPSILON, refracted);
            return Trace(refractedRay, depth + 1);
        }
    }

    case MaterialType::Emissive:
        // Emissive surfaces are their own light source — no further tracing needed
        return mat.emission * mat.emissionStr;
    }

    // Fallback: magenta indicates an unhandled material type
    return float3(1, 0, 1);
}

/* old version
float3 Renderer::Trace( Ray& ray, int depth, int, int )
{
scene.FindNearest( ray );
if (ray.voxel == 0) return float3( 0.5f, 0.6f, 1.0f ); // or a fancy sky color
float3 N = ray.GetNormal();
float3 I = ray.IntersectionPoint();
float3 albedo = ray.GetAlbedo();
static const float3 L = normalize( float3( 3, 2, 1 ) );
return albedo * max( 0.3f, dot( N, L ) );
}
**/


// -----------------------------------------------------------
/// @brief  One-time application initialisation, called before the first Tick.
///
/// Allocates the accumulator, sample-count buffer, history buffer, and blue-noise
/// texture.  Also constructs all light objects and adds them to the light list.
// -----------------------------------------------------------
void Renderer::Init()
{
    std::cout << "screen width: " << SCRWIDTH << " screen height: " << SCRHEIGHT << std::endl;

    // Per-pixel sample counter used by the temporal accumulator
    sampleCountPerPixel = new int[SCRWIDTH * SCRHEIGHT]();

    // -------------------------
    // Load the blue-noise texture (used for jittered sampling)
    // -------------------------
    // We store it as uint8_t (single channel) rather than in a Surface (uint32_t)
    // to save memory — we only need the red channel.
    Surface* bn = new Surface("assets/BlueNoise256x256.png");

    assert(bn->width == BN_SIZE && bn->height == BN_SIZE);

    blueNoise = new uint8_t[BN_SIZE * BN_SIZE];

    for (int i = 0; i < BN_SIZE * BN_SIZE; i++)
    {
        uint p = bn->pixels[i];
        blueNoise[i] = (uint8_t)((p >> 16) & 255); // extract the red channel
    }

    delete bn; // the Surface is no longer needed after extraction

    // -------------------------
    // Construct lights
    // -------------------------

    // Point light (disabled by default — toggle in ImGui)
    pointLight = new PointLight({ 1,1,1 }, { 1,1,1 });
    pointLight->enabled = false;

    // The directional light, spotlight, and area light are left commented out
    // below as reference.  Active lights are assembled into the lights list.

    //dirLight = new DirectionalLight({ 0.5f, -0.7f,0.45f }, { 1,1,1 });
    //dirLight->enabled = true;

    //spotLight = new SpotLight({ 1.5f,1.5f,1.4f }, { -0.57f,-0.58f,-0.5f }, { 1,1,0.8f }, 10.f);
    //spotLight->enabled = true;

    //float3 center = float3(0, 5, 0);
    //float3 edge1 = float3(4, 0, 0);   // width
    //float3 edge2 = float3(0, 0, 2);   // height
    //float3 corner = center - edge1 * 0.5f - edge2 * 0.5f;
    //areaLight = new AreaLight(corner, edge1, edge2, float3(10.0f, 10.0f, 10.0f), 16, 16);
    //areaLight->enabled = false;

    // Active light list: sky sun and moon are owned by the sky system
    lights = { &sky.sun, &sky.moon, pointLight };

    // -------------------------
    // Allocate and zero the frame buffers
    // -------------------------

    // HDR accumulator — stores running average of path-traced samples
    accumulator = new float3[SCRWIDTH * SCRHEIGHT];
    memset(accumulator, 0, SCRWIDTH * SCRHEIGHT * sizeof(float3));

    // History buffer — previous frame's output, used for temporal reprojection
    history = new float3[SCRWIDTH * SCRHEIGHT];
    memset(history, 0, SCRWIDTH * SCRHEIGHT * sizeof(float3));
}

// -----------------------------------------------------------
/// @brief  Per-frame update: traces one sample per pixel, blends with history,
///         and writes the final LDR result to the screen buffer.
///
/// Uses OpenMP to parallelise the pixel loop.  Temporal accumulation is handled
/// in two modes:
///   - Stationary camera: direct 1/N averaging (samples accumulate indefinitely).
///   - Moving camera: bilinear reprojection from history with neighbourhood clamp.
///
/// @param deltaTime Elapsed time since the last frame, in seconds.
// -----------------------------------------------------------
void Renderer::Tick(float deltaTime)
{
    auto startTime = std::chrono::high_resolution_clock::now();
    sampleCount++;
    int totalRaysThisFrame = 0;

    // Detect camera movement once before the pixel loop (not per-pixel)
    bool cameraMoving = length(camera.camPos - prevCamera.camPos) > 1e-4f ||
        length(camera.camTarget - prevCamera.camTarget) > 1e-4f;

#pragma omp parallel for schedule(dynamic)
    for (int y = 0; y < SCRHEIGHT; y++) for (int x = 0; x < SCRWIDTH; x++)
    {
        const int idx = x + y * SCRWIDTH;

        // -------------------------
        // Generate a jittered primary ray using blue-noise offsets
        // -------------------------
        float jx = BlueNoise(x, y, sampleCount);
        float jy = BlueNoise(y, x, sampleCount);
        Ray r = camera.GetPrimaryRay(x + jx, y + jy);

        float3 sample = Trace(r, 0, 0, 0); // trace the primary ray
        totalRaysThisFrame++;

        float3 blended;

        if (!cameraMoving)
        {
            if (r.voxel == 0)
            {
                // Sky pixel — do not accumulate; write the raw sample
                blended = sample;
            }
            else
            {
                // Geometry pixel — reset and store the new sample (fully opaque blend)
                blended = sample;
                sampleCountPerPixel[idx] = 1; // reset so future frames can accumulate
            }
        }
        else if (r.voxel > 0)
        {
            // -------------------------
            // Temporal reprojection: find where this world point was last frame
            // -------------------------
            float3 P = r.O + r.t * r.D; // world-space hit position
            float prev_x, prev_y;

            if (prevCamera.WorldToScreen(P, prev_x, prev_y))
            {
                // Bilinear sample of the history buffer at the reprojected position
                int   ix = static_cast<int>(prev_x);
                int   iy = static_cast<int>(prev_y);
                float fx = prev_x - ix; // fractional x offset
                float fy = prev_y - iy; // fractional y offset

                // Four neighbouring history texels
                float3 a = history[ix + iy * SCRWIDTH];
                float3 b = history[(ix + 1) + iy * SCRWIDTH];
                float3 c = history[ix + (iy + 1) * SCRWIDTH];
                float3 d = history[(ix + 1) + (iy + 1) * SCRWIDTH];

                // Bilinear interpolation
                float3 historySample = (1 - fx) * (1 - fy) * a + fx * (1 - fy) * b
                    + (1 - fx) * fy * c + fx * fy * d;

                // -------------------------
                // Neighbourhood clamp: restrict history to the local colour range to
                // reduce ghosting artefacts after disocclusion
                // -------------------------
                float3 lo = sample, hi = sample;
                for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++)
                {
                    if (dx == 0 && dy == 0) continue;
                    int nx = x + dx, ny = y + dy;
                    if (nx < 0 || nx >= SCRWIDTH || ny < 0 || ny >= SCRHEIGHT) continue;
                    float3 n = accumulator[nx + ny * SCRWIDTH];
                    lo = fminf(lo, n); // expand AABB minimum
                    hi = fmaxf(hi, n); // expand AABB maximum
                }
                historySample = clamp(historySample, lo, hi); // clamp history into bbox

                // Blend: 80 % history, 15 % new sample (implicit 5 % weight budget)
                blended = 0.8f * historySample + 0.15f * sample;
                sampleCountPerPixel[idx] = 6; // approximate effective sample count
            }
            else
            {
                // Reprojection went off-screen — treat as a new pixel
                blended = sample;
                sampleCountPerPixel[idx] = 1;
            }
        }
        else
        {
            // Sky pixel while moving — write raw sample, no accumulation
            blended = sample;
            sampleCountPerPixel[idx] = 1;
        }

        // Write the blended result to both the accumulator and the display buffer
        accumulator[idx] = blended;
        screen->pixels[idx] = RGBF32_to_RGB8(blended); // tone-map and pack to 8-bit
    }

    // Save the current camera state so it can be used as prevCamera next frame
    prevCamera = camera;

    // Advance the sky simulation (sun/moon position, sky colours, etc.)
    sky.Update(deltaTime);

    // Process keyboard / mouse input and update the camera transform
    camera.HandleInput(deltaTime);

    // Ping-pong the accumulator and history pointers so the current frame
    // becomes the history for the next frame
    swap(history, accumulator);

    // -------------------------
    // Performance counters
    // -------------------------
    auto endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float> frameDuration = endTime - startTime;
    lastFrameTime = frameDuration.count();
    avgFrameTimeMs = lastFrameTime * 1000.0f;
    fps = 1.0f / lastFrameTime;
    rps = (float)totalRaysThisFrame / (lastFrameTime * 1000000.0f); // Mrays/s

    // Rebuild the sphere BVH if a sphere was added or removed this frame
    if (rebuildSphereBVH)
    {
        scene.BuildSphereBVH();
        rebuildSphereBVH = false;
    }
}

// -----------------------------------------------------------
/// @brief  Builds the ImGui inspector panel.
///
/// Contains runtime stats, camera controls, sky settings, per-light editors,
/// material editors, and a sphere spawner utility.
// -----------------------------------------------------------
void Renderer::UI()
{
    ImGui::Begin("Inspector");

    // ===== RUNTIME STATS =====
    ImGui::BeginChild("Stats", ImVec2(0, 90), true);
    ImGui::Text("Renderer");
    ImGui::Separator();
    // Show the voxel value under the mouse cursor (uses a pinhole ray for stability)
    ImGui::Text("Voxel: %i",
        camera.GetPinholeRay(static_cast<float>(mousePos.x), static_cast<float>(mousePos.y)).voxel);
    ImGui::Text("%.2f ms | %.1f FPS", avgFrameTimeMs, fps);
    ImGui::Text("%.1f Mrays/s", rps);
    ImGui::EndChild();

    ImGui::Spacing();

    // ===== DEBUG =====
    if (ImGui::CollapsingHeader("Debug", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Indent();
        // Toggle normal visualisation; resets accumulator to avoid mixing debug and lit frames
        if (ImGui::Checkbox("Show Normals", &debugNormals))
            ResetAccumulator();
        ImGui::Unindent();

        // ===== CAMERA DEBUG =====
        if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent();
            bool cameraChanged = false; // tracks whether any camera parameter changed

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
                // Panini projection parameters (only shown when fisheye is off)
                cameraChanged |= ImGui::SliderFloat("Panini d", &camera.panini_d, 0.0f, 1.5f, "%.3f");
                cameraChanged |= ImGui::SliderFloat("Panini s", &camera.panini_s, 0.0f, 1.0f, "%.3f");
            }

            // Any camera change invalidates accumulated samples
            if (cameraChanged) ResetAccumulator();

            ImGui::Unindent();
        }
    }

    ImGui::Spacing();

    // ===== SKY =====
    if (ImGui::CollapsingHeader("Sky", ImGuiTreeNodeFlags_DefaultOpen))
    {
        bool skyChanged = false; // tracks whether any sky parameter changed

        skyChanged |= ImGui::SliderFloat("Time of Day", &sky.timeOfDay, 0.0f, 1.0f, "%.3f");
        skyChanged |= ImGui::SliderFloat("Cycle Speed", &sky.cycleSpeed, 0.0f, 0.1f, "%.4f");
        skyChanged |= ImGui::Checkbox("Animate", &sky.animate);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("Sun");
        skyChanged |= ImGui::ColorEdit3("Sun Color (noon)", &sky.sunNoonColor.x);
        skyChanged |= ImGui::ColorEdit3("Sun Color (horizon)", &sky.sunHorizonColor.x);
        skyChanged |= ImGui::SliderFloat("Sun Intensity", &sky.sunIntensity, 0.0f, 10.0f);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("Moon");
        skyChanged |= ImGui::ColorEdit3("Moon Color", &sky.moonColor.x);
        skyChanged |= ImGui::SliderFloat("Moon Intensity", &sky.moonIntensity, 0.0f, 1.0f);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("Sky Colors");
        skyChanged |= ImGui::ColorEdit3("Zenith", &sky.zenithColor.x);
        skyChanged |= ImGui::ColorEdit3("Horizon", &sky.horizonColor.x);

        // Manual time scrubbing also invalidates the accumulator
        if (skyChanged) ResetAccumulator();
    }
    ImGui::Spacing();

    // ===== LIGHTS =====
    if (ImGui::CollapsingHeader("Lights", ImGuiTreeNodeFlags_DefaultOpen))
    {
        bool lightsChanged = false;
        int  index = 0;

        for (Light* light : lights)
        {
            ImGui::PushID(index++);

            // Draw a collapsing header; open by default if the light is enabled
            bool open = ImGui::CollapsingHeader("##lightHeader",
                light->enabled ? ImGuiTreeNodeFlags_DefaultOpen : 0);

            // Enable / disable checkbox aligned to the right of the header
            ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 24);
            if (ImGui::Checkbox("##enabled", &light->enabled))
                lightsChanged = true;

            ImGui::SameLine(30);
            ImGui::TextUnformatted(LightTypeName(light)); // display the light type name

            if (open)
            {
                ImGui::Indent();
                ImGui::BeginDisabled(!light->enabled); // grey-out controls if disabled
                bool lightChangedThis = false;

                // Per-type property editors (dynamic dispatch via downcasting)
                if (auto* pl = dynamic_cast<PointLight*>(light))
                {
                    lightChangedThis |= ImGui::DragFloat3("Position", &pl->position.x, 0.1f);
                    lightChangedThis |= ImGui::ColorEdit3("Color", &pl->color.x);
                }
                else if (auto* dl = dynamic_cast<DirectionalLight*>(light))
                {
                    lightChangedThis |= ImGui::DragFloat3("Direction", &dl->direction.x, 0.01f);
                    if (lightChangedThis) dl->direction = normalize(dl->direction); // keep unit length
                    lightChangedThis |= ImGui::ColorEdit3("Color", &dl->color.x);
                }
                else if (auto* sl = dynamic_cast<SpotLight*>(light))
                {
                    lightChangedThis |= ImGui::DragFloat3("Position", &sl->position.x, 0.1f);
                    lightChangedThis |= ImGui::DragFloat3("Direction", &sl->direction.x, 0.01f);
                    if (lightChangedThis) sl->direction = normalize(sl->direction); // keep unit length
                    lightChangedThis |= ImGui::ColorEdit3("Color", &sl->color.x);
                    lightChangedThis |= ImGui::DragFloat("Range", &sl->range, 0.1f, 0.1f, 100.0f);
                    lightChangedThis |= ImGui::DragFloat("Angle", &sl->spotAngleDeg, 0.1f, 0.1f, 90.0f);
                    lightChangedThis |= ImGui::SliderFloat("Edge Softness", &sl->edgeRoughness, 0.0f, 1.0f);
                }
                else if (auto* al = dynamic_cast<AreaLight*>(light))
                {
                    lightChangedThis |= ImGui::ColorEdit3("Color", &al->color.x);
                    lightChangedThis |= ImGui::DragFloat("Intensity", &al->intensity, 0.1f, 0.0f, 1000.0f,
                        "%.1f", ImGuiSliderFlags_Logarithmic);
                    lightChangedThis |= ImGui::DragFloat3("Corner", &al->corner.x, 0.1f);
                    lightChangedThis |= ImGui::DragFloat3("Edge 1", &al->edge1.x, 0.1f);
                    lightChangedThis |= ImGui::DragFloat3("Edge 2", &al->edge2.x, 0.1f);
                }

                if (lightChangedThis) lightsChanged = true;

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

        // Show the currently locked material at the top when in selection mode
        if (selectionLocked && selectedMaterialIndex != -1)
        {
            if (MaterialUI("Selected Material", scene.materials[selectedMaterialIndex]))
                materialsChanged = true;

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
        }

        // Global / shared materials
        ImGui::Text("Global Materials");
        ImGui::Spacing();
        if (MaterialUI("Mirror", scene.materials[MAT_MIRROR]))     materialsChanged = true;
        if (MaterialUI("Dielectric", scene.materials[MAT_DIELECTRIC])) materialsChanged = true;

        if (materialsChanged) ResetAccumulator();
    }

    // ===== SPHERE SPAWNER =====
    if (ImGui::CollapsingHeader("Sphere Spawner", ImGuiTreeNodeFlags_DefaultOpen))
    {
        // Spawn configuration (static so values persist across frames)
        static float3 spawnMin = { 0.1f, 0.1f, 0.1f }; // minimum world-space corner
        static float3 spawnMax = { 0.9f, 0.9f, 0.9f }; // maximum world-space corner
        static float  spawnRadius = 0.05f;
        static int    spawnMatIndex = MAT_MIRROR;

        static const char* countLabels[] = { "1", "10", "100", "1000" };
        static const int   countValues[] = { 1,   10,   100,   1000 };
        static int selectedCount = 0;

        ImGui::Text("Spawn Range");
        ImGui::DragFloat3("Min XYZ", &spawnMin.x, 0.01f, 0.0f, 1.0f, "%.2f");
        ImGui::DragFloat3("Max XYZ", &spawnMax.x, 0.01f, 0.0f, 1.0f, "%.2f");

        // Prevent min from exceeding max
        spawnMin.x = min(spawnMin.x, spawnMax.x - 0.01f);
        spawnMin.y = min(spawnMin.y, spawnMax.y - 0.01f);
        spawnMin.z = min(spawnMin.z, spawnMax.z - 0.01f);

        ImGui::Spacing();
        ImGui::SliderFloat("Radius", &spawnRadius, 0.005f, 0.2f, "%.3f");

        // Material picker
        static const char* matNames[] = { "Mirror", "Dielectric", "Green" };
        static const uint  matIndices[] = { (uint)MAT_MIRROR, (uint)MAT_DIELECTRIC, (uint)MAT_GREEN };
        static int selectedMat = 0;
        ImGui::Combo("Material", &selectedMat, matNames, IM_ARRAYSIZE(matNames));
        spawnMatIndex = matIndices[selectedMat];

        ImGui::Spacing();
        ImGui::Text("Count");
        ImGui::SameLine();

        // Toggle buttons for spawn count (highlighted when selected)
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
            scene.BuildSphereBVH(); // rebuild immediately so the BVH is not stale
            ResetAccumulator();
        }

        // Note: the second "Clear Spheres" button below is a duplicate from the original code.
        ImGui::SameLine();
        if (ImGui::Button("Clear Spheres", ImVec2(-1, 0)))
        {
            scene.spheres.clear();
            ResetAccumulator();
        }
    }

    ImGui::End();
}

// -----------------------------------------------------------
/// @brief  Draws an ImGui tree-node editor for a single material.
///
/// Shows shared properties (type, albedo) and then type-specific
/// parameters (roughness, IOR, emission, etc.).
///
/// @param label    Display name shown in the tree-node header.
/// @param material The material to inspect and potentially modify.
/// @return         True if any property was changed this frame.
// -----------------------------------------------------------
bool Renderer::MaterialUI(const char* label, Material& material)
{
    ImGui::PushID(label); // ensure widget IDs don't collide across multiple calls
    bool changed = false;

    if (ImGui::TreeNode(label))
    {
        // Material type selector
        static const char* TypeLabels[] = { "Lambertian", "Metal", "Dielectric", "Emissive" };
        int type = static_cast<int>(material.type);
        if (ImGui::Combo("Shader", &type, TypeLabels, IM_ARRAYSIZE(TypeLabels)))
        {
            material.type = static_cast<MaterialType>(type);
            changed = true;
        }

        changed |= ImGui::ColorEdit3("Albedo", &material.albedo.x);

        // Type-specific parameters
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
            changed |= ImGui::SliderFloat("IOR", &material.ior, 1.0f, 2.5f); // typical range: air=1 to diamond=2.42
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

/// @brief  Allocates the accumulator buffer if it doesn't exist, then resets it.
void Tmpl8::Renderer::InitAccumulator()
{
    if (!accumulator)
    {
        // Aligned allocation for SIMD-friendly access patterns
        accumulator = static_cast<float3*>MALLOC64(SCRWIDTH * SCRHEIGHT * sizeof(float3));
    }
    ResetAccumulator();
}

/// @brief  Zeroes the accumulator, sample counters, and frame index so
///         temporal accumulation starts fresh.  Call whenever the scene or
///         camera changes.
// Claude helped
void Tmpl8::Renderer::ResetAccumulator()
{
    memset(accumulator, 0, SCRWIDTH * SCRHEIGHT * sizeof(float3));
    memset(sampleCountPerPixel, 0, SCRWIDTH * SCRHEIGHT * sizeof(int));
    sampleCount = 0;
}

/// @brief  Returns a human-readable name for the given light's dynamic type.
/// @param  light  Pointer to any Light subclass.
/// @return Null-terminated string literal with the type name, or "Unknown Light".
const char* Renderer::LightTypeName(Light* light)
{
    if (dynamic_cast<PointLight*>(light))       return "Point Light";
    if (dynamic_cast<DirectionalLight*>(light)) return "Directional Light";
    if (dynamic_cast<SpotLight*>(light))        return "Spot Light";
    if (dynamic_cast<AreaLight*>(light))        return "Area Light";
    return "Unknown Light";
}

/// @brief  Handles mouse button events for material selection.
///
/// Left-click locks the material under the cursor into the inspector.
/// Right-click releases the selection lock.
///
/// @param button  0 = left mouse button, 1 = right mouse button.
void Renderer::MouseDown(int button)
{
    if (button == 0) // left click: select the material under the cursor
    {
        if (!selectionLocked)
        {
            // Use a pinhole ray for stable, jitter-free picking
            Ray r = camera.GetPinholeRay(static_cast<float>(mousePos.x),
                static_cast<float>(mousePos.y));
            scene.FindNearest(r);

            if (r.materialIndex != -1)
            {
                selectedMaterialIndex = r.materialIndex;
                selectionLocked = true; // lock the selection until right-click
            }
        }
    }
    else if (button == 1) // right click: release the material selection lock
    {
        selectedMaterialIndex = -1;
        selectionLocked = false;
    }
}