#pragma once
#include "Core/Lighting/PointLight.h"
#include "Core/Lighting/DirectionalLight.h"
#include "Core/Lighting/SpotLight.h"
#include "Core/Lighting/AreaLight.h"
#include <vector>
#include <string>
#include <functional>
namespace Tmpl8 { class Scene; }
struct VoxPlacement
{
	std::string voxFile;
	float3 position;
	float3 rotation;
	float3 scale;
	bool   flatten = false;
};
struct SpherePlacement
{
	float3 center;
	float  radius;
	uint   material;
};
struct SkySettings
{
	float3 sunDir = normalize(float3(0.4f, -0.7f, 0.3f));
	float3 sunColor = float3(1.0f, 0.95f, 0.8f);
	float  sunIntensity = 2.5f;
	float  timeOfDay = 0.25f;
	bool   animate = false;
};
struct SpawnerState
{
	float3 rangeMin = { 0.1f, 0.1f, 0.1f };
	float3 rangeMax = { 0.9f, 0.9f, 0.9f };
	float  radius = 0.02f;
	int    countChoice = 0;
	int    matChoice = 0;
	static constexpr int   countValues[] = { 1, 10, 100, 1000 };
	static constexpr int   NUM_COUNTS = 4;
};
class SceneDef
{
public:
	const char* name = "unnamed";
	bool useVoxelGrid = true;
	bool usePhysics = false;
	float3 camPos = float3(0.5f, 0.5f, -0.5f);
	float3 camTarget = float3(0.5f, 0.3f, 0.5f);
	SkySettings sky;
	std::vector<VoxPlacement>    voxObjects;
	std::vector<SpherePlacement> spheres;
	std::vector<PointLight>       pointLights;
	std::vector<DirectionalLight> dirLights;
	std::vector<SpotLight>        spotLights;
	std::vector<AreaLight>        areaLights;
	SpawnerState spawner;
	std::vector<float3> splinePoints;
	std::function<void(SceneDef&, Tmpl8::Scene&, std::function<void()>)> uiCallback = nullptr;
	std::function<void(SceneDef&, Tmpl8::Scene&, float, std::function<void()>)> tickCallback = nullptr;
	std::function<void(Tmpl8::Scene&)> gridBuilder = nullptr;

};

