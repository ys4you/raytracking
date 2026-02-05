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
float3 Renderer::Trace(Ray& ray, int, int, int)
{
	scene.FindNearest(ray);
	if (ray.voxel == 0) return float3(0.53f, 0.81f, 0.92f); // background color

	ShadingPoint sp;
	sp.position = ray.IntersectionPoint();
	sp.normal = ray.GetNormal();
	sp.albedo = ray.GetAlbedo();

	float3 result(0);

	for (Light* light : lights)
		if (light->enabled)
			result += light->Illuminate(sp, scene);

	return result;
}

// -----------------------------------------------------------
// Application initialization - Executed once, at app start
// -----------------------------------------------------------
void Renderer::Init()
{
	// Create lights
	pointLight = new PointLight({ 1,1,1 }, { 1,1,1 });
	pointLight->enabled = false;

	dirLight = new DirectionalLight({ 0.f,-1.f,0.f }, { 1,1,1 });
	dirLight->enabled = false;

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


	lights = { pointLight, dirLight, spotLight, areaLight };
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

