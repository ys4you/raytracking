// ============================================================
// InfinityMirrorScene.h — Infinity Mirror Cube Showcase
// ============================================================
// 6-sided sealed cube:
//   Front (z=0)  = partial-mirror dielectric (one-way mirror)
//   Other 5 faces = bright metal mirrors
//   LED edge strips + grid lines on all inner mirror faces
//
// The partial-mirror front reflects some interior light back in
// while still letting the camera see through.
// The sealed box prevents room light from flooding the interior.
//
// Register with:
//     mgr.AddScene(GameScenes::InfinityMirrorShowcase());
// ============================================================

#pragma once
#include "SceneManager.h"
#include "VoxelFactory.h"
#include "VoxelObject.h"


namespace GameScenes
{

	static constexpr uint8_t PAL_MIRROR = 240;
	static constexpr uint8_t PAL_LED_EDGE = 241;
	static constexpr uint8_t PAL_LED_GRID = 242;
	static constexpr uint8_t PAL_ONEWAY = 243;  // one-way mirror (partial-mirror glass)

	// ── Build the mirror box ──────────────────────────────────────

	inline VoxelObject BuildInfinityMirrorBox(int size = 40)
	{
		std::vector<uint8_t> voxels(size * size * size, 0);

		auto set = [&](int x, int y, int z, uint8_t v)
			{
				if (x >= 0 && x < size && y >= 0 && y < size && z >= 0 && z < size)
					voxels[x + y * size + z * size * size] = v;
			};

		const int hi = size - 1;
		const int spacing = 10;

		// ── 1. Walls (fully sealed — all 6 faces) ────────────────
		for (int a = 0; a < size; a++)
			for (int b = 0; b < size; b++)
			{
				set(a, b, 0, PAL_MIRROR);    // bottom
				set(a, b, hi, PAL_MIRROR);    // top
				set(a, 0, b, PAL_MIRROR);    // back
				set(a, hi, b, PAL_ONEWAY);    // front (camera faces this)
				set(0, a, b, PAL_MIRROR);    // left
				set(hi, a, b, PAL_MIRROR);    // right
			}

		// ── 2. Emissive edge strips (dotted, every 3rd voxel) ───────
		//for (int i = 1; i < hi; i++)
		//{
		//	if (i % 3 != 0) continue;  // skip 2 out of 3 voxels

		//	// Bottom edges
		//	set(i, 0, 0, PAL_LED_EDGE);  set(i, 0, hi, PAL_LED_EDGE);
		//	set(0, 0, i, PAL_LED_EDGE);  set(hi, 0, i, PAL_LED_EDGE);
		//	// Top edges
		//	set(i, hi, 0, PAL_LED_EDGE);  set(i, hi, hi, PAL_LED_EDGE);
		//	set(0, hi, i, PAL_LED_EDGE);  set(hi, hi, i, PAL_LED_EDGE);
		//	// Vertical edges
		//	set(0, i, 0, PAL_LED_EDGE);  set(hi, i, 0, PAL_LED_EDGE);
		//	set(0, i, hi, PAL_LED_EDGE);  set(hi, i, hi, PAL_LED_EDGE);
		//}

		// ── 3. Grid lines on all 5 mirror faces (both directions) ─
		//    (not on z=0 front face — that's the see-through glass)

		// LEFT wall (x=1)
		for (int y = spacing; y < hi; y += spacing)
			for (int z = 1; z < hi; z++)
				set(1, y, z, PAL_LED_GRID);
		for (int z = spacing; z < hi; z += spacing)
			for (int y = 1; y < hi; y++)
				set(1, y, z, PAL_LED_GRID);

		// RIGHT wall (x=hi-1)
		for (int y = spacing; y < hi; y += spacing)
			for (int z = 1; z < hi; z++)
				set(hi - 1, y, z, PAL_LED_GRID);
		for (int z = spacing; z < hi; z += spacing)
			for (int y = 1; y < hi; y++)
				set(hi - 1, y, z, PAL_LED_GRID);

		// TOP wall (y=hi-1)
		//for (int x = spacing; x < hi; x += spacing)
		//	for (int z = 1; z < hi; z++)
		//		set(x, hi - 1, z, PAL_LED_GRID);
		//for (int z = spacing; z < hi; z += spacing)
		//	for (int x = 1; x < hi; x++)
		//		set(x, hi - 1, z, PAL_LED_GRID);

		// BOTTOM wall (y=1)
		for (int x = spacing; x < hi; x += spacing)
			for (int z = 1; z < hi; z++)
				set(x, 1, z, PAL_LED_GRID);
		for (int z = spacing; z < hi; z += spacing)
			for (int x = 1; x < hi; x++)
				set(x, 1, z, PAL_LED_GRID);

		// BACK wall (z=hi-1)
		for (int x = spacing; x < hi; x += spacing)
			for (int y = 1; y < hi; y++)
				set(x, y, hi - 1, PAL_LED_GRID);
		for (int y = spacing; y < hi; y += spacing)
			for (int x = 1; x < hi; x++)
				set(x, y, hi - 1, PAL_LED_GRID);

		return VoxelObject((uint)size, (uint)size, (uint)size, voxels);
	}


	// ── Materials ─────────────────────────────────────────────────

	inline void SetupInfinityMirrorMaterials(Tmpl8::Scene& scene)
	{
		// Mirror — high reflectivity, slightly dimmed to prevent blowout
		{
			Material m;
			m.type = MaterialType::Metal;
			m.albedo = float3(0.85f, 0.85f, 0.88f);
			m.roughness = 0.0f;
			m.metallic = 1.0f;
			scene.materials[MAT_COUNT + (PAL_MIRROR - 1)] = m;
		}

		// One-way mirror — partial mirror via metallic floor
		// 35% reflects back in, 65% transmits to camera
		{
			Material m;
			m.type = MaterialType::Dielectric;
			m.albedo = float3(0.90f, 0.90f, 0.92f);
			m.ior = 1.02f;
			m.metallic = 0.35f;
			scene.materials[MAT_COUNT + (PAL_ONEWAY - 1)] = m;
		}

		// Edge LEDs — dim cyan (the tunnel effect comes from repetition, not brightness)
		{
			Material m;
			m.type = MaterialType::Emissive;
			m.albedo = float3(0.3f, 0.85f, 1.0f);
			m.emission = float3(0.3f, 0.85f, 1.0f);
			m.emissionStr = .15f;
			m.roughness = 0.0f;
			scene.materials[MAT_COUNT + (PAL_LED_EDGE - 1)] = m;
		}

		// Grid LEDs — dim cyan-white
		{
			Material m;
			m.type = MaterialType::Emissive;
			m.albedo = float3(0.7f, 0.9f, 1.0f);
			m.emission = float3(0.7f, 0.9f, 1.0f);
			m.emissionStr = 0.1f;
			m.roughness = 0.0f;
			scene.materials[MAT_COUNT + (PAL_LED_GRID - 1)] = m;
		}
	}


	// ── Rotation ──────────────────────────────────────────────────

	struct MirrorRotation
	{
		float angleX = 0.0f, angleY = 0.0f, angleZ = 0.0f;
		float speedX = 0.0f, speedY = 0.0f, speedZ = 0.0f; // start static so accumulator converges
		bool  initialized = false;

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

		float3 GetRotation() const { return float3(angleX, angleY, angleZ); }
	};


	// ============================================================
	// Scene definition
	// ============================================================

	inline SceneDef InfinityMirrorShowcase()
	{
		SceneDef s;
		s.name = "Infinity Mirror";

		s.voxObjects.push_back({
			"assets/Showcase/display_platform.vox",
			float3(256, 120, 256),
			float3(0, 0, 0),
			float3(1, 1, 1), true
			});

		s.camPos = float3(0.5f, 0.47f, 0.38f);    // closer to the box
		s.camTarget = float3(0.5f, 0.47f, 0.5f);   // looking straight in

		s.sky.sunDir = normalize(float3(0.2f, -0.5f, 0.3f));
		s.sky.sunColor = float3(0.4f, 0.35f, 0.3f);
		s.sky.sunIntensity = 0.02f;
		s.sky.timeOfDay = 0.85f;
		s.sky.animate = false;

		auto addLight = [&](float3 pos, float3 col)
			{
				PointLight l;
				l.position = pos;
				l.color = col;
				l.enabled = true;
				s.pointLights.push_back(l);
			};
		addLight(float3(0.5f, 0.85f, 0.5f), float3(0.02f, 0.02f, 0.02f));
		addLight(float3(0.5f, 0.45f, 0.1f), float3(0.02f, 0.02f, 0.02f));

		auto rot = std::make_shared<MirrorRotation>();

		s.tickCallback = [rot](SceneDef& def, Tmpl8::Scene& scene,
			float deltaTime, std::function<void()> resetAcc)
			{
				if (!rot->initialized)
				{
					rot->initialized = true;
					scene.voxelObjects.push_back(BuildInfinityMirrorBox(40));
					SetupInfinityMirrorMaterials(scene);

					int boxObjIdx = (int)scene.voxelObjects.size() - 1;
					VoxelFactory::CreateInstance(
						scene, boxObjIdx,
						float3(256, 240, 256),
						float3(0, 0, 0),     // glass face toward camera
						float3(1, 1, 1)
					);
					scene.RebuildDirtyInstances();
					if (resetAcc) resetAcc();
					return;
				}

				// Only update rotation if any speed is non-zero
				if (rot->speedX > 0.0f || rot->speedY > 0.0f || rot->speedZ > 0.0f)
				{
					if (scene.voxelInstances.empty()) return;
					rot->Tick(deltaTime);
					auto& inst = scene.voxelInstances[0];
					inst.rotation = float3(0, sinf(rot->angleY) * 0.15f, 0);
					inst.matricesDirty = true;
					scene.RebuildDirtyInstances();
					if (resetAcc) resetAcc();
				}
			};
			
		s.uiCallback = [rot](SceneDef& def, Tmpl8::Scene& scene,
			std::function<void()> resetAcc)
			{
				if (ImGui::CollapsingHeader("Infinity Mirror"))
				{
					ImGui::SliderFloat("Speed X", &rot->speedX, 0.0f, 1.0f, "%.2f rad/s");
					ImGui::SliderFloat("Speed Y", &rot->speedY, 0.0f, 1.0f, "%.2f rad/s");
					ImGui::SliderFloat("Speed Z", &rot->speedZ, 0.0f, 1.0f, "%.2f rad/s");
				}
			};

		return s;
	}

} // namespace GameScenes