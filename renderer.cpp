#include "template.h"

#include "Core/ShadingPoint.h"
#include "Core/Lighting/Light.h"
#include "Core/Lighting/PointLight.h"

// -----------------------------------------------------------
// Calculate light transport via a ray
// -----------------------------------------------------------
float3 Renderer::Trace(Ray& ray, int, int, int)
{
	scene.FindNearest(ray);
	if (ray.voxel == 0) return float3(0);

	ShadingPoint sp;
	sp.position = ray.IntersectionPoint();
	sp.normal = ray.GetNormal();
	sp.albedo = ray.GetAlbedo();

	float3 result(0);

	for (Light* light : lights)
		result += light->Illuminate(sp, scene);

	return result;
}


// -----------------------------------------------------------
// Application initialization - Executed once, at app start
// -----------------------------------------------------------
void Renderer::Init()
{
	lights.insert(lights.end(), {
		new PointLight({1,1,1}, {1,1,1}),
		new PointLight({1,1,1}, {1,0.8f,0.6f})
		});


}

// -----------------------------------------------------------
// Main application tick function - Executed every frame
// -----------------------------------------------------------
void Renderer::Tick( float deltaTime )
{
	// high-resolution timer, see template.h
	Timer t;
	// pixel loop: lines are executed as OpenMP parallel tasks (disabled in DEBUG)
#pragma omp parallel for schedule(dynamic)
	for (int y = 0; y < SCRHEIGHT; y++)
	{
		// trace a primary ray for each pixel on the line
		for (int x = 0; x < SCRWIDTH; x++)
		{
			Ray r = camera.GetPrimaryRay( (float)x, (float)y );
			float3 pixel = Trace( r );
			screen->pixels[x + y * SCRWIDTH] = RGBF32_to_RGB8( pixel );
		}
	}
	// performance report - running average - ms, MRays/s
	static float avg = 10, alpha = 1;
	avg = (1 - alpha) * avg + alpha * t.elapsed() * 1000;
	if (alpha > 0.05f) alpha *= 0.5f;
	float fps = 1000.0f / avg, rps = (SCRWIDTH * SCRHEIGHT) / avg;
	printf( "%5.2fms (%.1ffps) - %.1fMrays/s\n", avg, fps, rps / 1000 );
	// handle user input
	camera.HandleInput( deltaTime );
}

// -----------------------------------------------------------
// Update user interface (imgui)
// -----------------------------------------------------------
void Renderer::UI()
{
	// ray query on mouse
	Ray r = camera.GetPrimaryRay( (float)mousePos.x, (float)mousePos.y );
	scene.FindNearest( r );
	ImGui::Text( "voxel: %i", r.voxel );
}