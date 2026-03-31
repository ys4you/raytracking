#pragma once
// ============================================================
// GameScenes.h — Define your scenes here
// ============================================================
#include "SceneManager.h"


#include "LivingCubeScene.h"
#include "InfinityMirrorScene.h"
#include "GyroscopeScene.h"
#include "BrickmapScene.h"
#include "PulseGridScene.h"
#include "OrbitCloudScene.h"

namespace GameScenes
{

	// ============================================================
	// Shared state for all showcase scenes.
	// ============================================================

	struct ShowcaseRotation
	{
		float angleX = 0.0f;
		float angleY = 0.0f;
		float angleZ = 0.0f;
		float speedX = 0.12f;
		float speedY = 0.25f;
		float speedZ = 0.08f;

		void Tick(float deltaTimeMs)
		{
			float dt = deltaTimeMs * 0.001f;
			angleX += speedX * dt;
			angleY += speedY * dt;
			angleZ += speedZ * dt;
			if (angleX > 2.0f * PI) angleX -= 2.0f * PI;
			if (angleY > 2.0f * PI) angleY -= 2.0f * PI;
			if (angleZ > 2.0f * PI) angleZ -= 2.0f * PI;
		}

		float3 GetRotation() const
		{
			return float3(angleX, angleY, angleZ);
		}
	};

	inline std::shared_ptr<ShowcaseRotation>& SharedRotation()
	{
		static auto s = std::make_shared<ShowcaseRotation>();
		return s;
	}


	// ============================================================
	// Shared showcase setup — sky, camera, lights, callbacks.
	// ============================================================

	inline void SetupShowcase(SceneDef& s)
	{
		s.camPos = float3(0.5f, 0.46f, 0.25f);
		s.camTarget = float3(0.5f, 0.45f, 0.5f);

		s.sky.sunDir = normalize(float3(0.2f, -0.5f, 0.3f));
		s.sky.sunColor = float3(1.0f, 0.85f, 0.8f);
		s.sky.sunIntensity = 1.2f;
		s.sky.timeOfDay = 0.3f;
		s.sky.animate = false;

		PointLight overhead;
		overhead.position = float3(0.5f, 0.85f, 0.5f);
		overhead.color = float3(1.8f, 1.75f, 1.7f);
		overhead.enabled = true;
		s.pointLights.push_back(overhead);

		PointLight front;
		front.position = float3(0.5f, 0.45f, 0.1f);
		front.color = float3(1.0f, 0.95f, 0.9f);
		front.enabled = true;
		s.pointLights.push_back(front);

		PointLight leftFill;
		leftFill.position = float3(0.1f, 0.5f, 0.5f);
		leftFill.color = float3(0.4f, 0.55f, 0.5f);
		leftFill.enabled = true;
		s.pointLights.push_back(leftFill);

		PointLight rightFill;
		rightFill.position = float3(0.9f, 0.5f, 0.5f);
		rightFill.color = float3(0.45f, 0.4f, 0.55f);
		rightFill.enabled = true;
		s.pointLights.push_back(rightFill);

		PointLight below;
		below.position = float3(0.5f, 0.15f, 0.5f);
		below.color = float3(0.3f, 0.28f, 0.26f);
		below.enabled = true;
		s.pointLights.push_back(below);

		PointLight behind;
		behind.position = float3(0.5f, 0.5f, 0.9f);
		behind.color = float3(0.3f, 0.38f, 0.45f);
		behind.enabled = true;
		s.pointLights.push_back(behind);

		auto rot = SharedRotation();

		s.tickCallback = [rot](SceneDef& def, Tmpl8::Scene& scene,
			float deltaTime, std::function<void()> resetAcc)
			{
				if (scene.voxelInstances.empty()) return;

				rot->Tick(deltaTime);
				auto& inst = scene.voxelInstances[0];
				inst.rotation = rot->GetRotation();
				inst.matricesDirty = true;

				scene.RebuildDirtyInstances();
				if (resetAcc) resetAcc();
			};

		s.uiCallback = [rot](SceneDef& def, Tmpl8::Scene& scene,
			std::function<void()> resetAcc)
			{
				if (ImGui::CollapsingHeader("Showcase Settings"))
				{
					ImGui::SliderFloat("Speed X", &rot->speedX, 0.0f, 1.0f, "%.2f rad/s");
					ImGui::SliderFloat("Speed Y", &rot->speedY, 0.0f, 1.0f, "%.2f rad/s");
					ImGui::SliderFloat("Speed Z", &rot->speedZ, 0.0f, 1.0f, "%.2f rad/s");
				}
			};
	}


	// ============================================================
	// 01 CUBE
	// ============================================================

	inline SceneDef CubeShowcase()
	{
		SceneDef s;
		s.name = "01 CUBE";
		SetupShowcase(s);

		s.voxObjects.push_back({
			"assets/Showcase/display_platform.vox",
			float3(256, 120, 256),
			float3(0, 0, 0),
			float3(1, 1, 1), true
			});

		s.voxObjects.push_back({
			"assets/Showcase/display_cube.vox",
			float3(256, 240, 256),
			float3(0, 0, 0),
			float3(1, 1, 1)
			});

		return s;
	}


	// ============================================================
	// 02 MENGER
	// ============================================================

	inline SceneDef MengerShowcase()
	{
		SceneDef s;
		s.name = "02 MENGER";
		SetupShowcase(s);

		s.voxObjects.push_back({
			"assets/Showcase/display_platform.vox",
			float3(256, 120, 256),
			float3(0, 0, 0),
			float3(1, 1, 1), true
			});

		s.voxObjects.push_back({
			"assets/Showcase/menger_sponge.vox",
			float3(256, 245, 256),
			float3(0, 0, 0),
			float3(1, 1, 1)
			});

		return s;
	}


	// ============================================================
	// Sphere Spawner UI helper
	// ============================================================
	inline void SphereSpawnerUI(SceneDef& def, Tmpl8::Scene& scene, std::function<void()> resetAcc)
	{
		if (!ImGui::CollapsingHeader("Sphere Spawner"))
			return;

		SpawnerState& sp = def.spawner;

		ImGui::DragFloat3("Min", &sp.rangeMin.x, 0.01f, 0.0f, 1.0f, "%.2f");
		ImGui::DragFloat3("Max", &sp.rangeMax.x, 0.01f, 0.0f, 1.0f, "%.2f");
		sp.rangeMin.x = min(sp.rangeMin.x, sp.rangeMax.x - 0.01f);
		sp.rangeMin.y = min(sp.rangeMin.y, sp.rangeMax.y - 0.01f);
		sp.rangeMin.z = min(sp.rangeMin.z, sp.rangeMax.z - 0.01f);

		ImGui::SliderFloat("Radius", &sp.radius, 0.005f, 0.2f, "%.3f");

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

		int sphereCount = (int)scene.spheres.size();
		if (sphereCount > 0 && ImGui::TreeNode("Spheres##list"))
		{
			ImGui::Text("%d sphere%s", sphereCount, sphereCount == 1 ? "" : "s");

			int deleteIdx = -1;
			int showMax = min(sphereCount, 200);

			if (sphereCount > showMax)
				ImGui::TextDisabled("(showing first %d of %d)", showMax, sphereCount);

			for (int i = 0; i < showMax; i++)
			{
				ImGui::PushID(i);
				auto& sph = scene.spheres[i];

				ImGui::Text("#%d", i);
				ImGui::SameLine(40);
				ImGui::TextDisabled("(%.2f, %.2f, %.2f)  r=%.3f  m=%u",
					sph.center.x, sph.center.y, sph.center.z,
					sph.radius, sph.material);
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
	// Physics Demo — balls rolling on voxel terrain
	// ============================================================
	inline SceneDef PhysicsDemo()
	{
		SceneDef s;
		s.name = "Physics";
		s.useVoxelGrid = true;
		s.usePhysics = true;

		s.camPos = float3(0.5f, 0.65f, -0.2f);
		s.camTarget = float3(0.5f, 0.25f, 0.5f);

		s.sky.sunDir = normalize(float3(0.4f, -0.7f, 0.3f));
		s.sky.sunColor = float3(1.0f, 0.95f, 0.8f);
		s.sky.sunIntensity = 2.5f;
		s.sky.timeOfDay = 0.25f;

		s.gridBuilder = [](Tmpl8::Scene& scene)
			{
				for (int x = 0; x < 512; x++)
					for (int z = 0; z < 512; z++)
						scene.SetVoxel(x, 64, z, 200); 
			};
		
		// Spawn balls at the top of the ramp
		const int NUM_BALLS = 12;
		for (int i = 0; i < NUM_BALLS; i++)
		{
			float xOff = (i % 4) * 0.04f - 0.06f;
			float zOff = (i / 4) * 0.04f;

			uint mat = (i % 3 == 0) ? MAT_MIRROR
				: (i % 3 == 1) ? MAT_DIELECTRIC
				: MAT_GREEN;

			s.spheres.push_back({
				float3(0.5f + xOff, 0.45f + 0.02f * i, 0.45f + zOff),
				0.015f,
				mat
				});
		}

		s.spawner.radius = 0.015f;
		s.spawner.rangeMin = float3(0.35f, 0.50f, 0.40f);
		s.spawner.rangeMax = float3(0.65f, 0.55f, 0.50f);

		// Lights
		PointLight overhead;
		overhead.position = float3(0.5f, 0.85f, 0.5f);
		overhead.color = float3(1.5f, 1.45f, 1.4f);
		overhead.enabled = true;
		s.pointLights.push_back(overhead);

		PointLight front;
		front.position = float3(0.5f, 0.4f, 0.1f);
		front.color = float3(0.6f, 0.58f, 0.55f);
		front.enabled = true;
		s.pointLights.push_back(front);

		s.uiCallback = [](SceneDef& def, Tmpl8::Scene& scene, std::function<void()> resetAcc)
			{
				SphereSpawnerUI(def, scene, resetAcc);
			};

		return s;
	}

	// ============================================================
	// Register all scenes
	// ============================================================

	inline void RegisterAllScenes(SceneManager& mgr)
	{

		mgr.AddScene(CubeShowcase());
		mgr.AddScene(MengerShowcase());

		mgr.AddScene(LivingCubeShowcase());
		mgr.AddScene(PulseGridShowcase());
		mgr.AddScene(OrbitCloudShowcase());


		mgr.AddScene(BrickmapShowcase());
		mgr.AddScene(InfinityMirrorShowcase());

		//gyroscope
		mgr.AddScene(GyroscopeShowcase());
		mgr.AddScene(PhysicsDemo());
	}

} // namespace GameScenes