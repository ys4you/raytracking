#pragma once
// ============================================================
// GameScenes.h — Define your scenes here
// ============================================================
#include "SceneManager.h"

namespace GameScenes
{

	// ============================================================
	// Helper: Sphere Spawner UI — reusable across scenes
	// ============================================================
	inline void SphereSpawnerUI(SceneDef& def, Tmpl8::Scene& scene, std::function<void()> resetAcc)
	{
		if (!ImGui::CollapsingHeader("Sphere Spawner"))
			return;

		SpawnerState& sp = def.spawner;

		// ── Spawn range ────────────────────────────────────────
		ImGui::DragFloat3("Min", &sp.rangeMin.x, 0.01f, 0.0f, 1.0f, "%.2f");
		ImGui::DragFloat3("Max", &sp.rangeMax.x, 0.01f, 0.0f, 1.0f, "%.2f");
		sp.rangeMin.x = min(sp.rangeMin.x, sp.rangeMax.x - 0.01f);
		sp.rangeMin.y = min(sp.rangeMin.y, sp.rangeMax.y - 0.01f);
		sp.rangeMin.z = min(sp.rangeMin.z, sp.rangeMax.z - 0.01f);

		// ── Radius ─────────────────────────────────────────────
		ImGui::SliderFloat("Radius", &sp.radius, 0.005f, 0.2f, "%.3f");

		// ── Material selector with color preview ───────────────
		struct MatOption { const char* name; uint id; float3 color; };
		static const MatOption matOpts[] =
		{
			{ "Mirror",     MAT_MIRROR,     float3(0.9f, 0.9f, 0.95f) },
			{ "Dielectric", MAT_DIELECTRIC, float3(0.8f, 0.9f, 1.0f)  },
			{ "Green",      MAT_GREEN,      float3(0.2f, 0.8f, 0.3f)  },
		};
		static constexpr int NUM_MATS = sizeof(matOpts) / sizeof(matOpts[0]);

		const MatOption& cur = matOpts[sp.matChoice];
		ImGui::ColorButton("##matClr", ImVec4(cur.color.x, cur.color.y, cur.color.z, 1),
			ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker,
			ImVec2(14, 14));
		ImGui::SameLine();
		if (ImGui::BeginCombo("Material", cur.name))
		{
			for (int i = 0; i < NUM_MATS; i++)
			{
				ImGui::ColorButton("##opt", ImVec4(matOpts[i].color.x, matOpts[i].color.y,
					matOpts[i].color.z, 1),
					ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker,
					ImVec2(12, 12));
				ImGui::SameLine();
				if (ImGui::Selectable(matOpts[i].name, sp.matChoice == i))
					sp.matChoice = i;
			}
			ImGui::EndCombo();
		}

		// ── Count selector ─────────────────────────────────────
		ImGui::Spacing();
		static const char* countLabels[] = { "1", "10", "100", "1000" };

		ImGui::Text("Count");
		ImGui::SameLine();
		for (int i = 0; i < SpawnerState::NUM_COUNTS; i++)
		{
			if (i > 0) ImGui::SameLine();
			bool active = (sp.countChoice == i);
			if (active) ImGui::PushStyleColor(ImGuiCol_Button,
				ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
			if (ImGui::Button(countLabels[i], ImVec2(40, 0)))
				sp.countChoice = i;
			if (active) ImGui::PopStyleColor();
		}

		// ── Spawn / Clear buttons ──────────────────────────────
		ImGui::Spacing();
		float halfW = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

		if (ImGui::Button("Spawn", ImVec2(halfW, 0)))
		{
			int n = SpawnerState::countValues[sp.countChoice];
			uint matID = matOpts[sp.matChoice].id;
			scene.spheres.reserve(scene.spheres.size() + n);
			for (int i = 0; i < n; i++)
			{
				scene.spheres.push_back(Sphere{
					float3(
						sp.rangeMin.x + RandomFloat() * (sp.rangeMax.x - sp.rangeMin.x),
						sp.rangeMin.y + RandomFloat() * (sp.rangeMax.y - sp.rangeMin.y),
						sp.rangeMin.z + RandomFloat() * (sp.rangeMax.z - sp.rangeMin.z)
					),
					sp.radius,
					matID
					});
			}
			scene.BuildSphereBVH();
			if (resetAcc) resetAcc();
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear All", ImVec2(halfW, 0)))
		{
			scene.spheres.clear();
			scene.BuildSphereBVH();
			if (resetAcc) resetAcc();
		}

		// ── Sphere list with individual delete ──────────────────
		int sphereCount = (int)scene.spheres.size();
		if (sphereCount > 0 && ImGui::TreeNode("Spheres##list"))
		{
			ImGui::Text("%d sphere%s", sphereCount, sphereCount == 1 ? "" : "s");

			// Only show individual entries for manageable counts
			int deleteIdx = -1;
			int showMax = min(sphereCount, 200);

			if (sphereCount > showMax)
				ImGui::TextDisabled("(showing first %d of %d)", showMax, sphereCount);

			for (int i = 0; i < showMax; i++)
			{
				ImGui::PushID(i);
				auto& s = scene.spheres[i];

				// Compact single-line display
				ImGui::Text("#%d", i);
				ImGui::SameLine(40);
				ImGui::TextDisabled("(%.2f, %.2f, %.2f)  r=%.3f  m=%u",
					s.center.x, s.center.y, s.center.z,
					s.radius, s.material);
				ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 20);
				if (ImGui::SmallButton("X"))
					deleteIdx = i;

				ImGui::PopID();
			}

			if (deleteIdx >= 0)
			{
				scene.spheres.erase(scene.spheres.begin() + deleteIdx);
				scene.BuildSphereBVH();
				if (resetAcc) resetAcc();
			}
			ImGui::TreePop();
		}

		// ── BVH mode toggle ────────────────────────────────────
		ImGui::Spacing();
		if (ImGui::Checkbox("Use BVH (legacy)", &scene.useLegacyBVH))
		{
			scene.BuildSphereBVH();
			if (resetAcc) resetAcc();
		}
		ImGui::SameLine();
		ImGui::TextDisabled(scene.useLegacyBVH ? "(SAH BVH2)" : "(uniform grid)");
	}


	// ============================================================
	// Scene definitions
	// ============================================================

	inline SceneDef ThousandSpheres()
	{
		SceneDef s;
		s.name = "ThousandSpheres";

		s.camPos = float3(0.5f, 0.5f, -0.5f);
		s.camTarget = float3(0.5f, 0.3f, 0.5f);

		s.sky.sunDir = normalize(float3(0.4f, -0.7f, 0.3f));
		s.sky.sunColor = float3(1.0f, 0.95f, 0.8f);
		s.sky.sunIntensity = 2.5f;
		s.sky.timeOfDay = 0.25f;

		// Pre-populate 1000 random spheres
		for (int i = 0; i < 1000; i++)
		{
			uint mat = MAT_RANDOM_START +
				static_cast<uint>(RandomFloat() * (MAT_RANDOM_END - MAT_RANDOM_START));
			s.spheres.push_back({
				float3(0.1f + RandomFloat() * 0.8f,
					   0.1f + RandomFloat() * 0.8f,
					   0.1f + RandomFloat() * 0.8f),
				0.01f,
				mat
				});
		}

		// Default spawner settings for this scene
		s.spawner.radius = 0.01f;

		PointLight pl;
		pl.position = float3(1, 1, 1);
		pl.color = float3(1, 1, 1);
		pl.enabled = false;
		s.pointLights.push_back(pl);

		s.uiCallback = [](SceneDef& def, Tmpl8::Scene& scene, std::function<void()> resetAcc)
			{
				SphereSpawnerUI(def, scene, resetAcc);
			};

		return s;
	}


	inline SceneDef TestScene()
	{
		SceneDef s;
		s.name = "Test";

		s.camPos = float3(0.5f, 0.8f, -0.5f);
		s.camTarget = float3(0.5f, 0.2f, 0.5f);

		s.sky.sunDir = normalize(float3(-0.3f, -0.8f, 0.5f));
		s.sky.sunColor = float3(1.0f, 1.0f, 0.95f);
		s.sky.sunIntensity = 3.0f;
		s.sky.timeOfDay = 0.25f;

		s.spheres.push_back({ float3(0.5f, 0.5f, 0.5f), 0.1f,  MAT_DIELECTRIC });
		s.spheres.push_back({ float3(0.3f, 0.4f, 0.4f), 0.07f, MAT_MIRROR });

		PointLight overhead;
		overhead.position = float3(0.5f, 0.9f, 0.5f);
		overhead.color = float3(1, 1, 1);
		overhead.enabled = true;
		s.pointLights.push_back(overhead);

		SpotLight spot;
		spot.position = float3(0.2f, 0.7f, 0.2f);
		spot.direction = normalize(float3(0.3f, -1.0f, 0.3f));
		spot.color = float3(0.8f, 0.85f, 1.0f);
		spot.spotAngleDeg = 35.0f;
		spot.enabled = true;
		s.spotLights.push_back(spot);

		s.uiCallback = [](SceneDef& def, Tmpl8::Scene& scene, std::function<void()> resetAcc)
			{
				if (ImGui::TreeNode("Test Scene"))
				{
					ImGui::Text("Spheres: %d", (int)scene.spheres.size());
					ImGui::TreePop();
				}
			};

		return s;
	}


	inline void RegisterAllScenes(SceneManager& mgr)
	{
		mgr.AddScene(ThousandSpheres());
		mgr.AddScene(TestScene());
	}

} // namespace GameScenes