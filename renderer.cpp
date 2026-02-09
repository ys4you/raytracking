#include "template.h"
#include "Core/ShadingPoint.h"
#include "Core/Lighting/Light.h"
#include "Core/Lighting/PointLight.h"
#include "Core/Lighting/DirectionalLight.h"
#include "Core/Lighting/SpotLight.h"
#include "Core/Lighting/AreaLight.h"



// -----------------------------------------------------------
// Calculate light transport via a ray
// -----------------------------------------------------------
float3 Renderer::Trace(Ray& ray, int depth, int, int)
{
    const int MAX_DEPTH = 5;
    if (depth >= MAX_DEPTH) return float3(0, 0, 0);

    // Find nearest intersection
    scene.FindNearest(ray);

    // Background (sky)
    if (ray.voxel == 0)
        return float3(0.53f, 0.81f, 0.92f);

    // Shading info
    ShadingPoint sp;
    sp.position = ray.IntersectionPoint();
    sp.normal = ray.GetNormal();
    sp.albedo = ray.GetAlbedo();

    const Material& mat = ray.hitMaterial;

    // ---------------------------
    // Debug: show normals only
    // ---------------------------
    if (debugNormals)
    {
        // Map from [-1,1] to [0,1]
        return 0.5f * (sp.normal + float3(1.0f));
    }

    float3 result(0);

    // ---------------------------
    // Handle material types
    // ---------------------------
    switch (mat.type)
    {
    case MaterialType::Lambertian:
    {
        // Accumulate lighting
        for (Light* light : lights)
            if (light->enabled)
                result += light->Illuminate(sp, scene);

        // Multiply by albedo
        return result * mat.albedo;
    }

    case MaterialType::Metal:
    {
        float3 N = sp.normal;
        float3 R = normalize(ray.D - 2.0f * dot(ray.D, N) * N);

        // Add roughness (diffuse reflection)
        if (mat.roughness > 0.0f)
            R += mat.roughness * RandomInUnitSphere();
        R = normalize(R);

        Ray aRay(sp.position + N * EPSILON, R);
        // Trace reflection
        return Trace(aRay, depth + 1) * mat.albedo;
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
            Ray Ray(sp.position + N * EPSILON, reflect(I, N));
            return Trace(Ray, depth + 1);
        }
        else
        {
            Ray Ray(sp.position - N * EPSILON, refracted);
            return Trace(Ray, depth + 1);
        }
    }

    case MaterialType::Emissive:
        return mat.emission * mat.emissionStr;
    }

    // Fallback debug color
    return float3(1, 0, 1);
}

/*
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
	// Create lights
	pointLight = new PointLight({ 1,1,1 }, { 1,1,1 });
	pointLight->enabled = false;

	dirLight = new DirectionalLight({ 0.3f, -0.35f,0.9f }, { 1,1,1 });
    dirLight->enabled = true;

	spotLight = new SpotLight({ 1.5f,1.5f,1.4f }, { -0.57f,-0.58f,-0.5f }, { 1,1,0.8f }, 10.f);
	spotLight->enabled = false;

    float3 center = float3(0, 5, 0);

    float3 edge1 = float3(4, 0, 0);   // width
    float3 edge2 = float3(0, 0, 2);   // height

    float3 corner = center - edge1 * 0.5f - edge2 * 0.5f;

    areaLight = new AreaLight(
        corner,
        edge1,
        edge2,
        float3(10.0f, 10.0f, 10.0f), // bright white (area lights need energy)  with 1,1,1... lights are dim
        16, 16                         // 16 samples total
    );

    areaLight->enabled = false;

	lights = { pointLight, dirLight, spotLight, areaLight };

    //accumulator
    accumulator = new float3[SCRWIDTH * SCRHEIGHT];
    memset(accumulator, 0, SCRWIDTH * SCRHEIGHT * sizeof(float3));
}

// -----------------------------------------------------------
// Main application tick function - Executed every frame
// -----------------------------------------------------------
void Renderer::Tick(float deltaTime)
{
	static Timer frameTimer;
	float frameTime = frameTimer.elapsed(); // time since last frame
	frameTimer.reset();

	// pixel loop (OpenMP can be enabled for parallelism)
#pragma omp parallel for schedule(dynamic)
	for (int y = 0; y < SCRHEIGHT; y++)
	{
		for (int x = 0; x < SCRWIDTH; x++)
		{
			Ray r = camera.GetPrimaryRay((float)x, (float)y);
			float3 pixel = Trace(r);
			screen->pixels[x + y * SCRWIDTH] = RGBF32_to_RGB8(pixel);
		}
	}

	// handle user input
	camera.HandleInput(deltaTime);
}

// -----------------------------------------------------------
// Update user interface (imgui)
// -----------------------------------------------------------
void Renderer::UI()
{
    // Ray query on mouse
    Ray r = camera.GetPrimaryRay((float)mousePos.x, (float)mousePos.y);
    scene.FindNearest(r);
    ImGui::Text("voxel: %i", r.voxel);
    ImGui::Separator();
    ImGui::Checkbox("Show Normals", &debugNormals);
    ImGui::Separator();
    ImGui::Text("Lights");

    int lightIndex = 0;
    for (Light* light : lights)
    {
        ImGui::PushID(lightIndex++);
        ImGui::Checkbox("Enabled", &light->enabled);

        if (PointLight* pl = dynamic_cast<PointLight*>(light))
        {
            if (ImGui::CollapsingHeader("Point Light##Header", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::DragFloat3("Position", &pl->position.x, 0.1f);
                ImGui::ColorEdit3("Color", &pl->color.x);
            }
        }
        else if (DirectionalLight* dl = dynamic_cast<DirectionalLight*>(light))
        {
            if (ImGui::CollapsingHeader("Directional Light##Header", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::DragFloat3("Direction", &dl->direction.x, 0.01f);
                dl->direction = normalize(dl->direction);
                ImGui::ColorEdit3("Color", &dl->color.x);
            }
        }
        else if (SpotLight* sl = dynamic_cast<SpotLight*>(light))
        {
            if (ImGui::CollapsingHeader("Spot Light##Header", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::DragFloat3("Position", &sl->position.x, 0.1f);
                ImGui::DragFloat3("Direction", &sl->direction.x, 0.01f);
                sl->direction = normalize(sl->direction);
                ImGui::ColorEdit3("Color", &sl->color.x);
                ImGui::DragFloat("Range", &sl->range, 0.1f, 0.1f, 100.0f);
                ImGui::DragFloat("Angle", &sl->spotAngleDeg, 0.1f, 0.1f, 90.0f);
                ImGui::DragFloat("Edge Roughness", &sl->edgeRoughness, 0.01f, 0.0f, 1.0f);
                sl->edgeRoughness = clamp(sl->edgeRoughness, 0.0f, 0.99f);
            }
        }
        else if (AreaLight* al = dynamic_cast<AreaLight*>(light))
        {
            if (ImGui::CollapsingHeader("Area Light##Header", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::ColorEdit3("Color", &al->color.x);   // stays 0–1
                ImGui::DragFloat("Intensity", &al->intensity, 0.1f, 0.0f, 1000.0f);
                ImGui::Spacing();
                ImGui::DragFloat3("Corner", &al->corner.x, 0.1f);
                ImGui::DragFloat3("Edge 1", &al->edge1.x, 0.1f);
                ImGui::DragFloat3("Edge 2", &al->edge2.x, 0.1f);
            }
        }
        ImGui::PopID();
    }

    ImGui::Separator();

    // Display FPS and Mrays/sec in ImGui
    ImGui::Text("%5.2f ms (%.1f FPS) - %.1f Mrays/s", avgFrameTimeMs, fps, rps);
}

