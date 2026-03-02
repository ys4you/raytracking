#include "template.h"
#include "Core/ShadingPoint.h"
#include "Core/Lighting/Light.h"
#include "Core/Lighting/PointLight.h"
#include "Core/Lighting/DirectionalLight.h"
#include "Core/Lighting/SpotLight.h"
#include "Core/Lighting/AreaLight.h"



// -----------------------------------------------------------
// Calculate light transport via a ray (handles voxels + spheres)
// -----------------------------------------------------------
float3 Renderer::Trace(Ray& ray, int depth, int, int)
{
    const int MAX_DEPTH = 5;
    if (depth >= MAX_DEPTH) return float3(0, 0, 0);

    // Find nearest intersection (voxels or spheres)
    scene.FindNearest(ray);

    // No hit: return sky color
    if (ray.voxel == 0 && ray.sphereIndex < 0)
        return sky.GetSkyColor(ray.D);

    // Determine material
    int matIndex = -1;
    if (ray.sphereIndex >= 0)
        matIndex = scene.spheres[ray.sphereIndex].material;
	else if (ray.voxel > 0)
        matIndex = ray.materialIndex;

    if (matIndex < 0 || matIndex >= TOTAL_MATS)
        return float3(1, 0, 1); // invalid material

    const Material& mat = scene.materials[matIndex];

    // Build shading point
    ShadingPoint sp;
    sp.position = ray.IntersectionPoint();
    sp.normal = ray.GetNormal(scene); // works for both voxels & spheres
    sp.albedo = ray.GetAlbedo(scene);

    if (debugNormals)
        return 0.5f * (sp.normal + float3(1.0f));

    // -------------------------
    // Material shading
    // -------------------------
    switch (mat.type)
    {
    case MaterialType::Lambertian:
    {
        float3 color(0);
        for (Light* light : lights)
            if (light->enabled)
                color += light->Illuminate(sp, scene) * mat.albedo;
        return color;
    }

    case MaterialType::Metal:
    {
        float3 N = sp.normal;
        float3 R = normalize(ray.D - 2.0f * dot(ray.D, N) * N);
        if (mat.roughness > 0.0f)
            R += mat.roughness * RandomInUnitSphere();
        R = normalize(R);

        Ray reflectedRay(sp.position + N * EPSILON, R);
        return Trace(reflectedRay, depth + 1) * mat.albedo;
    }

    case MaterialType::Dielectric:
    {
        float3 N = sp.normal;
        float3 I = normalize(ray.D);
        float3 refracted;
        float ni_over_nt = dot(I, N) > 0 ? mat.ior : 1.0f / mat.ior;
        float reflect_prob = 1.0f;

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

    // Fallback color
    return float3(1, 0, 1);
}
/* old version 
float3 Renderer::Trace( Ray& ray, int depth, int, int )w
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
// Application initialization - Executed once, at app start
// -----------------------------------------------------------
void Renderer::Init()
{
    sampleCountPerPixel = new int[SCRWIDTH * SCRHEIGHT]();

    //blue noise
	//I am puting it in an uint8_t rather then keeping it in surface since surface has a uint21_t.
    //That would be more bytes for each first frame to load. less efficient
    Surface* bn = new Surface("assets/BlueNoise256x256.png");

    assert(bn->width == BN_SIZE && bn->height == BN_SIZE);

    blueNoise = new uint8_t[BN_SIZE * BN_SIZE];

    for (int i = 0; i < BN_SIZE * BN_SIZE; i++)
    {
        uint p = bn->pixels[i];
        blueNoise[i] = (uint8_t)((p >> 16) & 255); // take R channel
    }

    delete bn;



	// Create lights
	pointLight = new PointLight({ 1,1,1 }, { 1,1,1 });
	pointLight->enabled = false;

	//dirLight = new DirectionalLight({ 0.5f, -0.7f,0.45f }, { 1,1,1 });
 //   dirLight->enabled = true;

	//spotLight = new SpotLight({ 1.5f,1.5f,1.4f }, { -0.57f,-0.58f,-0.5f }, { 1,1,0.8f }, 10.f);
	//spotLight->enabled = true;

    //float3 center = float3(0, 5, 0);

    //float3 edge1 = float3(4, 0, 0);   // width
    //float3 edge2 = float3(0, 0, 2);   // height

    //float3 corner = center - edge1 * 0.5f - edge2 * 0.5f;

    //areaLight = new AreaLight(
    //    corner,
    //    edge1,
    //    edge2,
    //    float3(10.0f, 10.0f, 10.0f), // bright white (area lights need energy)  with 1,1,1... lights are dim
    //    16, 16                         // 16 samples total
    //);

    //areaLight->enabled = false;

    //lights = { pointLight, dirLight, spotLight, areaLight };
    lights = { &sky.sun, &sky.moon, pointLight};

    //accumulator
    accumulator = new float3[SCRWIDTH * SCRHEIGHT];
    memset(accumulator, 0, SCRWIDTH * SCRHEIGHT * sizeof(float3));

    //history
    history = new float3[SCRWIDTH * SCRHEIGHT];
    memset(history, 0, SCRWIDTH * SCRHEIGHT * sizeof(float3));


}

// -----------------------------------------------------------
// Main application tick function - Executed every frame
// -----------------------------------------------------------
void Renderer::Tick(float deltaTime)
{
    auto startTime = std::chrono::high_resolution_clock::now();
    sampleCount++;
    int totalRaysThisFrame = 0;

    // Detect camera movement ONCE before the loop, not per-pixel
    bool cameraMoving = length(camera.camPos - prevCamera.camPos) > 1e-4f ||
        length(camera.camTarget - prevCamera.camTarget) > 1e-4f;

#pragma omp parallel for schedule(dynamic)
    for (int y = 0; y < SCRHEIGHT; y++) for (int x = 0; x < SCRWIDTH; x++)
    {
        const int idx = x + y * SCRWIDTH;

        // Jittered primary ray
        float jx = BlueNoise(x, y, sampleCount);
        float jy = BlueNoise(y, x, sampleCount);
        Ray r = camera.GetPrimaryRay(x + jx, y + jy);
        float3 sample = Trace(r, 0, 0, 0);
        totalRaysThisFrame++;

        float3 blended;

        if (!cameraMoving)
        {
            if (r.voxel == 0) // Sky pixel
                blended = sample;
            else              // Geometry pixel — opaque: overwrite history
            {
                blended = sample; // <- fully opaque
                sampleCountPerPixel[idx] = 1; // reset sample count for this pixel
            }
        }
        else if (r.voxel > 0)
        {
            float3 P = r.O + r.t * r.D;
            float prev_x, prev_y;

            if (prevCamera.WorldToScreen(P, prev_x, prev_y))
            {
                // Bilinear sample from history at reprojected position
                int ix = (int)prev_x;
                int iy = (int)prev_y;
                float fx = prev_x - ix;
                float fy = prev_y - iy;

                float3 a = history[ix + iy * SCRWIDTH];
                float3 b = history[(ix + 1) + iy * SCRWIDTH];
                float3 c = history[ix + (iy + 1) * SCRWIDTH];
                float3 d = history[(ix + 1) + (iy + 1) * SCRWIDTH];

                float3 historySample = (1 - fx) * (1 - fy) * a + fx * (1 - fy) * b
                    + (1 - fx) * fy * c + fx * fy * d;

                // Clamp historySample to neighborhood bounding box to reduce ghosting
                float3 lo = sample, hi = sample;
                for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++)
                {
                    if (dx == 0 && dy == 0) continue;
                    int nx = x + dx, ny = y + dy;
                    if (nx < 0 || nx >= SCRWIDTH || ny < 0 || ny >= SCRHEIGHT) continue;
                    float3 n = accumulator[nx + ny * SCRWIDTH];
                    lo = fminf(lo, n);
                    hi = fmaxf(hi, n);
                }
                historySample = clamp(historySample, lo, hi);
                blended = 0.8f * historySample + 0.15f * sample;
                sampleCountPerPixel[idx] = 6;

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

    prevCamera = camera;

    sky.Update(deltaTime);
    camera.HandleInput(deltaTime);

    swap(history, accumulator);

    auto endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float> frameDuration = endTime - startTime;
    lastFrameTime = frameDuration.count();
    avgFrameTimeMs = lastFrameTime * 1000.0f;
    fps = 1.0f / lastFrameTime;
    rps = (float)totalRaysThisFrame / (lastFrameTime * 1000000.0f);

    printf("MAT_COUNT = %d, MAT_GREEN = %d\n", MAT_COUNT, MAT_GREEN);
}

// ----------------------------------------------------------- 
// Update user interface (imgui) with accumulator reset
// -----------------------------------------------------------
void Renderer::UI()
{
    ImGui::Begin("Inspector");

    // ===== RUNTIME STATS =====
    ImGui::BeginChild("Stats", ImVec2(0, 90), true);
    ImGui::Text("Renderer");
    ImGui::Separator();
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
        if (ImGui::Checkbox("Show Normals", &debugNormals))
            ResetAccumulator();
        ImGui::Unindent();

        // ===== CAMERA DEBUG =====
        if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent();
            bool cameraChanged = false;

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

            if (cameraChanged) ResetAccumulator();

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

        // Manual time scrubbing resets accumulator too
        if (skyChanged) ResetAccumulator();
    }
    ImGui::Spacing();

    // ===== LIGHTS =====
    if (ImGui::CollapsingHeader("Lights", ImGuiTreeNodeFlags_DefaultOpen))
    {
        bool lightsChanged = false;
        int index = 0;
        for (Light* light : lights)
        {
            ImGui::PushID(index++);
            bool open = ImGui::CollapsingHeader("##lightHeader", light->enabled ? ImGuiTreeNodeFlags_DefaultOpen : 0);

            // Enable checkbox
            ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 24);
            if (ImGui::Checkbox("##enabled", &light->enabled))
                lightsChanged = true;

            ImGui::SameLine(30);
            ImGui::TextUnformatted(LightTypeName(light));

            if (open)
            {
                ImGui::Indent();
                ImGui::BeginDisabled(!light->enabled);
                bool lightChangedThis = false;

                if (auto* pl = dynamic_cast<PointLight*>(light))
                {
                    lightChangedThis |= ImGui::DragFloat3("Position", &pl->position.x, 0.1f);
                    lightChangedThis |= ImGui::ColorEdit3("Color", &pl->color.x);
                }
                else if (auto* dl = dynamic_cast<DirectionalLight*>(light))
                {
                    lightChangedThis |= ImGui::DragFloat3("Direction", &dl->direction.x, 0.01f);
                    if (lightChangedThis) dl->direction = normalize(dl->direction);
                    lightChangedThis |= ImGui::ColorEdit3("Color", &dl->color.x);
                }
                else if (auto* sl = dynamic_cast<SpotLight*>(light))
                {
                    lightChangedThis |= ImGui::DragFloat3("Position", &sl->position.x, 0.1f);
                    lightChangedThis |= ImGui::DragFloat3("Direction", &sl->direction.x, 0.01f);
                    if (lightChangedThis) sl->direction = normalize(sl->direction);
                    lightChangedThis |= ImGui::ColorEdit3("Color", &sl->color.x);
                    lightChangedThis |= ImGui::DragFloat("Range", &sl->range, 0.1f, 0.1f, 100.0f);
                    lightChangedThis |= ImGui::DragFloat("Angle", &sl->spotAngleDeg, 0.1f, 0.1f, 90.0f);
                    lightChangedThis |= ImGui::SliderFloat("Edge Softness", &sl->edgeRoughness, 0.0f, 1.0f);
                }
                else if (auto* al = dynamic_cast<AreaLight*>(light))
                {
                    lightChangedThis |= ImGui::ColorEdit3("Color", &al->color.x);
                    lightChangedThis |= ImGui::DragFloat("Intensity", &al->intensity, 0.1f, 0.0f, 1000.0f, "%.1f", ImGuiSliderFlags_Logarithmic);
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

        if (selectionLocked && selectedMaterialIndex != -1)
        {
            if (MaterialUI("Selected Material", scene.materials[selectedMaterialIndex]))
                materialsChanged = true;

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
        }

        ImGui::Text("Global Materials");
        ImGui::Spacing();
        if (MaterialUI("Mirror", scene.materials[MAT_MIRROR])) materialsChanged = true;
        if (MaterialUI("Dielectric", scene.materials[MAT_DIELECTRIC])) materialsChanged = true;
        //if (MaterialUI("Lambertian", scene.materials[MAT_LAMBERTIAN])) materialsChanged = true;

        if (materialsChanged) ResetAccumulator();
    }

    ImGui::End();
}

// -----------------------------------------------------------
// MaterialUI with change detection
// Returns true if any property changed
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

void Tmpl8::Renderer::InitAccumulator()
{
    if (!accumulator) 
    {
        accumulator =  static_cast<float3*>MALLOC64(SCRWIDTH * SCRHEIGHT * sizeof(float3));
    }
    ResetAccumulator();
}

//Claude helped
void Tmpl8::Renderer::ResetAccumulator()
{
    memset(accumulator, 0, SCRWIDTH * SCRHEIGHT * sizeof(float3));
    memset(sampleCountPerPixel, 0, SCRWIDTH * SCRHEIGHT * sizeof(int));
    sampleCount = 0;
}


const char* Renderer::LightTypeName(Light* light)
{
    if (dynamic_cast<PointLight*>(light))       return "Point Light";
    if (dynamic_cast<DirectionalLight*>(light)) return "Directional Light";
    if (dynamic_cast<SpotLight*>(light))        return "Spot Light";
    if (dynamic_cast<AreaLight*>(light))        return "Area Light";
    return "Unknown Light";
}


void Renderer::MouseDown(int button)
{
    if (button == 0) // left click: select material
    {
        if (!selectionLocked)
        {
            // Use pinhole ray for stable selection
            Ray r = camera.GetPinholeRay(static_cast<float>(mousePos.x), static_cast<float>(mousePos.y));
            scene.FindNearest(r);

            if (r.materialIndex != -1)
            {
                selectedMaterialIndex = r.materialIndex;
                selectionLocked = true;
            }
        }
    }
    else if (button == 1) // right click: unlock selection
    {
        selectedMaterialIndex = -1;
        selectionLocked = false;
    }
}
