#pragma once
#include "SceneManager.h"
#include "VoxelFactory.h"
#include "VoxelObject.h"


namespace GameScenes
{

	static constexpr uint8_t PAL_MIRROR = 244;
	static constexpr uint8_t PAL_LED_GRID = 245;
	static constexpr uint8_t PAL_ONEWAY = 246;


	inline VoxelObject BuildInfinityMirrorBox(int size = 40)
	{
		std::vector<uint8_t> voxels(size * size * size, 0);

		auto set = [&](int x, int y, int z, uint8_t v)
			{
				if (x >= 0 && x < size && y >= 0 && y < size && z >= 0 && z < size)
					voxels[x + y * size + z * size * size] = v;
			};

		const int hi = size - 1;
		const int spacing = 6;  // tighter grid = denser reflections

		// 5 mirror walls + 1 one-way glass top
		for (int a = 0; a < size; a++)
			for (int b = 0; b < size; b++)
			{
				set(a, b, 0, PAL_MIRROR);      // back
				set(a, b, hi, PAL_MIRROR);      // front
				set(a, 0, b, PAL_MIRROR);       // bottom
				set(a, hi, b, PAL_ONEWAY);      // top = one-way glass
				set(0, a, b, PAL_MIRROR);       // left
				set(hi, a, b, PAL_MIRROR);      // right
			}

		// LED grid on ALL 5 mirror inner surfaces

		// left wall (x=1)
		for (int y = spacing; y < hi; y += spacing)
			for (int z = 1; z < hi; z++)
				set(1, y, z, PAL_LED_GRID);
		for (int z = spacing; z < hi; z += spacing)
			for (int y = 1; y < hi; y++)
				set(1, y, z, PAL_LED_GRID);

		// right wall (x=hi-1)
		for (int y = spacing; y < hi; y += spacing)
			for (int z = 1; z < hi; z++)
				set(hi - 1, y, z, PAL_LED_GRID);
		for (int z = spacing; z < hi; z += spacing)
			for (int y = 1; y < hi; y++)
				set(hi - 1, y, z, PAL_LED_GRID);

		// bottom (y=1)
		for (int x = spacing; x < hi; x += spacing)
			for (int z = 1; z < hi; z++)
				set(x, 1, z, PAL_LED_GRID);
		for (int z = spacing; z < hi; z += spacing)
			for (int x = 1; x < hi; x++)
				set(x, 1, z, PAL_LED_GRID);

		// back (z=1)
		for (int x = spacing; x < hi; x += spacing)
			for (int y = 1; y < hi; y++)
				set(x, y, 1, PAL_LED_GRID);
		for (int y = spacing; y < hi; y += spacing)
			for (int x = 1; x < hi; x++)
				set(x, y, 1, PAL_LED_GRID);

		// front (z=hi-1)
		for (int x = spacing; x < hi; x += spacing)
			for (int y = 1; y < hi; y++)
				set(x, y, hi - 1, PAL_LED_GRID);
		for (int y = spacing; y < hi; y += spacing)
			for (int x = 1; x < hi; x++)
				set(x, y, hi - 1, PAL_LED_GRID);

		return VoxelObject((uint)size, (uint)size, (uint)size, voxels);
	}


	inline void SetupInfinityMirrorMaterials(Tmpl8::Scene& scene)
	{
		// mirror: near-perfect reflector, cool tint
		{
			Material m;
			m.type = MaterialType::Metal;
			m.albedo = float3(0.88f, 0.87f, 0.91f);
			m.roughness = 0.0f;
			m.metallic = 1.0f;
			scene.materials[MAT_COUNT + (PAL_MIRROR - 1)] = m;
		}

		// one-way glass: slight lavender tint
		{
			Material m;
			m.type = MaterialType::Dielectric;
			m.albedo = float3(0.91f, 0.88f, 0.95f);
			m.ior = 1.02f;
			m.metallic = 0.35f;
			scene.materials[MAT_COUNT + (PAL_ONEWAY - 1)] = m;
		}

		// LED grid: pale lavender, brighter than before
		{
			Material m;
			m.type = MaterialType::Emissive;
			m.albedo = float3(0.82f, 0.76f, 0.92f);
			m.emission = float3(0.784f, 0.722f, 0.847f);
			m.emissionStr = 0.15f;
			m.roughness = 0.0f;
			scene.materials[MAT_COUNT + (PAL_LED_GRID - 1)] = m;
		}
	}


	struct MirrorState
	{
		bool initialized = false;
		int  boxObjIdx = -1;
		float phase = 0.0f;

		void Tick(float dtMs)
		{
			float dt = dtMs * 0.001f;
			phase += 0.4f * dt;
			if (phase > 2.0f * PI) phase -= 2.0f * PI;
		}

		float3 GetRotDeg() const
		{
			float x = 8.0f + sinf(phase * 0.7f) * 6.0f;
			float y = sinf(phase) * 10.0f;
			float z = sinf(phase * 0.53f + 1.0f) * 5.0f;
			return float3(x, y, z);
		}
	};


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

		s.camPos = float3(0.50f, 0.53f, 0.30f);
		s.camTarget = float3(0.50f, 0.46f, 0.50f);
		s.splineSpeed = 0.03f;

		// small lateral sway and height shifts to catch reflections at different angles
		s.splinePoints = {
			float3(0.50f, 0.53f, 0.30f),
			float3(0.44f, 0.52f, 0.32f),
			float3(0.42f, 0.50f, 0.30f),
			float3(0.46f, 0.51f, 0.28f),
			float3(0.50f, 0.54f, 0.29f),
			float3(0.55f, 0.52f, 0.31f),
			float3(0.57f, 0.50f, 0.30f),
			float3(0.54f, 0.51f, 0.28f),
			float3(0.50f, 0.53f, 0.30f),
		};

		// very dark so LED reflections dominate
		s.sky.sunDir = normalize(float3(0.2f, -0.5f, 0.3f));
		s.sky.sunColor = float3(0.4f, 0.35f, 0.3f);
		s.sky.sunIntensity = 0.02f;
		s.sky.timeOfDay = 0.85f;
		s.sky.animate = false;

		auto addLight = [&](float3 pos, float3 col)
			{
				PointLight l;
				l.position = pos; l.color = col; l.enabled = true;
				s.pointLights.push_back(l);
			};
		// very dim fill — just enough to see the box silhouette
		addLight(float3(0.5f, 0.85f, 0.5f), float3(0.03f, 0.025f, 0.04f));
		addLight(float3(0.5f, 0.45f, 0.1f), float3(0.02f, 0.015f, 0.03f));

		auto st = std::make_shared<MirrorState>();

		s.tickCallback = [st](SceneDef& def, Tmpl8::Scene& scene,
			float deltaTime, std::function<void()> resetAcc)
			{
				if (!st->initialized)
				{
					st->initialized = true;
					scene.voxelObjects.push_back(BuildInfinityMirrorBox(40));
					SetupInfinityMirrorMaterials(scene);
					st->boxObjIdx = (int)scene.voxelObjects.size() - 1;
				}

				st->Tick(deltaTime);

				// rebuild instance each frame with current rotation
				scene.voxelInstances.clear();
				VoxelFactory::CreateInstance(
					scene, st->boxObjIdx,
					float3(256, 240, 256),
					st->GetRotDeg(),
					float3(1, 1, 1)
				);
				scene.RebuildDirtyInstances();
				if (resetAcc) resetAcc();
			};

		s.uiCallback = [st](SceneDef& def, Tmpl8::Scene& scene,
			std::function<void()> resetAcc)
			{
				if (!ImGui::CollapsingHeader("Infinity Mirror", ImGuiTreeNodeFlags_DefaultOpen))
					return;

				float3 r = st->GetRotDeg();
				ImGui::Text("Tilt: %.1f  %.1f  %.1f", r.x, r.y, r.z);
			};

		return s;
	}

}