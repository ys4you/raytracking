#include "template.h"
#include "SceneManager.h"
#include "scene.h"
#include "camera.h"
#include "Sky.h"
#include "renderer.h"     // for SceneLights
#include "VoxLoader.h"
#include "VoxelFactory.h"

int SceneManager::AddScene(const SceneDef& def)
{
	scenes.push_back(def);
	return (int)scenes.size() - 1;
}

SceneDef& SceneManager::Active()
{
	if (currentID >= 0 && currentID < (int)scenes.size())
		return scenes[currentID];
	return emptyScene;
}

const SceneDef& SceneManager::Active() const
{
	if (currentID >= 0 && currentID < (int)scenes.size())
		return scenes[currentID];
	return emptyScene;
}

// ============================================================
// LoadScene — clear everything, rebuild from SceneDef
// ============================================================
void SceneManager::LoadScene(int id, Tmpl8::Scene& worldScene,
	Tmpl8::Camera& camera, Sky& sky,
	SceneLights& lights)
{
	if (id < 0 || id >= (int)scenes.size())
	{
		printf("[SceneManager] Invalid scene ID %d\n", id);
		return;
	}

	SceneDef& def = scenes[id];
	printf("[SceneManager] Loading scene %d: '%s'\n", id, def.name);
	Timer t;

	// ---- 1. Clear world grid ----
	worldScene.ClearWorld();

	// ---- 2. Clear spheres ----
	worldScene.spheres.clear();

	// ---- 3. Clear voxel objects/instances ----
	worldScene.voxelObjects.clear();
	worldScene.voxelInstances.clear();

	// ---- 4. Load and flatten .vox objects ----
	for (const auto& obj : def.voxObjects)
	{
		if (obj.voxFile.empty()) continue;

		VoxLoader::Load(obj.voxFile.c_str(), worldScene);

		if (!worldScene.voxelObjects.empty())
		{
			int lastIdx = (int)worldScene.voxelObjects.size() - 1;
			float3 rotRad = obj.rotation * (PI / 180.0f);
			VoxelFactory::FlattenInstance(
				worldScene, lastIdx,
				obj.position, rotRad, obj.scale
			);
		}
	}

	// ---- 5. Place spheres ----
	for (const auto& s : def.spheres)
		worldScene.spheres.push_back({ s.center, s.radius, s.material });

	worldScene.BuildSphereBVH();

	// ---- 6. Camera ----
	camera.camPos = def.camPos;
	camera.camTarget = def.camTarget;

	// ---- 7. Sky ----
	sky.timeOfDay = def.sky.timeOfDay;
	sky.sunNoonColor = def.sky.sunColor;
	sky.sunIntensity = def.sky.sunIntensity;
	sky.skyCacheDirty = true;

	// ---- 8. Lights ----
	lights.points.clear();
	lights.directionals.clear();
	lights.spots.clear();
	lights.areas.clear();

	lights.directionals.push_back(sky.sun);
	lights.directionals.push_back(sky.moon);

	for (const auto& pl : def.pointLights)    lights.points.push_back(pl);
	for (const auto& dl : def.dirLights)      lights.directionals.push_back(dl);
	for (const auto& sl : def.spotLights)     lights.spots.push_back(sl);
	for (const auto& al : def.areaLights)     lights.areas.push_back(al);

	// ---- 9. Done ----
	currentID = id;
	printf("[SceneManager] Loaded '%s' in %.1fms  %d vox, %d spheres, %d lights\n",
		def.name, t.elapsed() * 1000.0f,
		(int)worldScene.voxelObjects.size(),
		(int)worldScene.spheres.size(),
		(int)(lights.points.size() + lights.directionals.size() +
			lights.spots.size() + lights.areas.size()));
}

// ============================================================
// UI — scene selector + per-scene editable params
// ============================================================
void SceneManager::UI(Tmpl8::Scene& worldScene, Tmpl8::Camera& camera,
	Sky& sky, SceneLights& lights,
	std::function<void()> resetAccumulator)
{
	if (!ImGui::CollapsingHeader("Scene Manager", ImGuiTreeNodeFlags_DefaultOpen))
		return;

	// ── Scene selector: tab-style buttons with F-key hints ──────────────
	ImGui::Text("Scenes");
	ImGui::SameLine();
	ImGui::TextDisabled("(F1-F%d)", (int)scenes.size());

	float btnWidth = (ImGui::GetContentRegionAvail().x - (float)(scenes.size() - 1) * ImGui::GetStyle().ItemSpacing.x) / (float)scenes.size();
	btnWidth = max(btnWidth, 60.0f);

	for (int i = 0; i < (int)scenes.size(); i++)
	{
		bool isCurrent = (i == currentID);

		if (isCurrent)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.55f, 0.25f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.65f, 0.30f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.10f, 0.45f, 0.20f, 1.0f));
		}

		char label[128];
		snprintf(label, sizeof(label), "%s###scene_%d", scenes[i].name, i);

		if (ImGui::Button(label, ImVec2(btnWidth, 0)))
			LoadScene(i, worldScene, camera, sky, lights);

		if (isCurrent) ImGui::PopStyleColor(3);
		if (i < (int)scenes.size() - 1) ImGui::SameLine();
	}

	if (!HasActive()) return;
	SceneDef& def = Active();

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	// ── Scene info bar ──────────────────────────────────────────────────
	ImGui::TextDisabled("Active:");
	ImGui::SameLine();
	ImGui::Text("%s", def.name);
	ImGui::SameLine(ImGui::GetContentRegionAvail().x - 120);
	ImGui::TextDisabled("%d sph | %d lgt",
		(int)worldScene.spheres.size(),
		(int)(lights.points.size() + lights.directionals.size() +
			lights.spots.size() + lights.areas.size()));

	ImGui::Spacing();

	// ── Camera ──────────────────────────────────────────────────────────
	if (ImGui::TreeNode("Camera##scn"))
	{
		bool changed = false;
		changed |= ImGui::DragFloat3("Position", &def.camPos.x, 0.01f);
		changed |= ImGui::DragFloat3("Target", &def.camTarget.x, 0.01f);

		if (changed)
		{
			camera.camPos = def.camPos;
			camera.camTarget = def.camTarget;
		}
		ImGui::TreePop();
	}

	// ── Sky ─────────────────────────────────────────────────────────────
	if (ImGui::TreeNode("Sky##scn"))
	{
		bool changed = false;
		changed |= ImGui::DragFloat3("Sun Dir", &def.sky.sunDir.x, 0.01f, -1.0f, 1.0f);
		changed |= ImGui::ColorEdit3("Sun Color", &def.sky.sunColor.x);
		changed |= ImGui::DragFloat("Sun Intensity", &def.sky.sunIntensity, 0.1f, 0.0f, 20.0f);
		changed |= ImGui::SliderFloat("Time of Day", &def.sky.timeOfDay, 0.0f, 1.0f);

		if (changed)
		{
			sky.sunNoonColor = def.sky.sunColor;
			sky.sunIntensity = def.sky.sunIntensity;
			sky.timeOfDay = def.sky.timeOfDay;
			sky.skyCacheDirty = true;
		}
		ImGui::TreePop();
	}

	// ── Per-scene custom UI ─────────────────────────────────────────────
	if (def.uiCallback)
		def.uiCallback(def, worldScene, resetAccumulator);
}