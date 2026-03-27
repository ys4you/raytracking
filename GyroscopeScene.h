// ============================================================
// GyroscopeScene.h — Multi-Axis Gyroscope: 1000 Spheres
// ============================================================
// Three orbital rings on different tilted axes rotate around a
// single large flickering red emissive warning sphere.
//
// Style: Side Order sterile minimalist dystopia.
//   ~80% matte off-white Lambertian
//   ~10% chrome Metal
//   ~5%  Dielectric glass
//   ~3%  muted pastel Emissive (coral pink, pale lavender, muted teal)
//   1    large center sphere — slowly flickering red emissive
//
// Register with:
//     mgr.AddScene(GameScenes::GyroscopeShowcase());
// ============================================================

#pragma once
#include "SceneManager.h"

namespace GameScenes
{

	// ── Material IDs (indexes into scene.materials[]) ─────────
	// Using slots in the random material range — overwritten at init.
	static constexpr uint MAT_GYRO_WHITE = MAT_RANDOM_START;
	static constexpr uint MAT_GYRO_CHROME = MAT_RANDOM_START + 1;
	static constexpr uint MAT_GYRO_GLASS = MAT_RANDOM_START + 2;
	static constexpr uint MAT_GYRO_EMIT_ROSE = MAT_RANDOM_START + 3;
	static constexpr uint MAT_GYRO_EMIT_LAV = MAT_RANDOM_START + 4;
	static constexpr uint MAT_GYRO_EMIT_TEAL = MAT_RANDOM_START + 5;
	static constexpr uint MAT_GYRO_CENTER = MAT_RANDOM_START + 6;


	// ── Gyroscope animation state ─────────────────────────────

	struct GyroState
	{
		// Ring definitions
		struct Ring
		{
			float tilt;       // radians — rotation of the ring plane around X
			float speed;      // radians/s — orbital speed
			int   count;      // spheres on this ring
			float radius;     // orbital radius in world units
		};

		static constexpr int NUM_RINGS = 5;

		Ring rings[NUM_RINGS] = {
			{ 0.10f,  0.30f, 200, 0.40f },   // near-horizontal
			{ 0.80f, -0.22f, 200, 0.36f },   // tilted, reverse
			{ 1.50f,  0.40f, 200, 0.32f },   // steep tilt
			{ 2.20f, -0.35f, 200, 0.38f },   // another axis, reverse
			{ 2.90f,  0.28f, 200, 0.34f },   // fifth axis
		};

		float time = 0.0f;
		bool  initialized = false;

		// Flickering
		float flickerPhase = 0.0f;

		// Sphere radius
		float orbitSphereRadius = 0.005f;
		float centerSphereRadius = 0.14f;

		// UI-tweakable
		float globalSpeed = 1.0f;

		// Pre-assigned material per sphere (set once at init, stable across frames)
		std::vector<uint> sphereMaterials;

		void AssignMaterials()
		{
			sphereMaterials.clear();
			int total = 0;
			for (int i = 0; i < NUM_RINGS; i++) total += rings[i].count;
			sphereMaterials.reserve(total);

			// Deterministic seed for reproducible look
			uint seed = 42;
			for (int i = 0; i < total; i++)
			{
				float r = RandomFloat(seed);
				uint mat;
				if (r < 0.80f) mat = MAT_GYRO_WHITE;
				else if (r < 0.90f) mat = MAT_GYRO_CHROME;
				else if (r < 0.95f) mat = MAT_GYRO_GLASS;
				else if (r < 0.97f) mat = MAT_GYRO_EMIT_ROSE;
				else if (r < 0.99f) mat = MAT_GYRO_EMIT_LAV;
				else                mat = MAT_GYRO_EMIT_TEAL;
				sphereMaterials.push_back(mat);
			}
		}
	};


	// ── Setup materials ───────────────────────────────────────

	inline void SetupGyroscopeMaterials(Tmpl8::Scene& scene)
	{
		// Off-white Lambertian — Side Order's primary surface
		// Slightly warm: RGB 248/247/244 → ~(0.973, 0.969, 0.957)
		{
			Material m;
			m.type = MaterialType::Lambertian;
			m.albedo = float3(0.973f, 0.969f, 0.957f);
			m.roughness = 0.35f;
			scene.materials[MAT_GYRO_WHITE] = m;
		}

		// Chrome Metal — clinical, reflective
		{
			Material m;
			m.type = MaterialType::Metal;
			m.albedo = float3(0.92f, 0.92f, 0.94f);
			m.roughness = 0.02f;
			m.metallic = 1.0f;
			scene.materials[MAT_GYRO_CHROME] = m;
		}

		// Dielectric glass — ghost spheres
		{
			Material m;
			m.type = MaterialType::Dielectric;
			m.albedo = float3(0.98f, 0.98f, 1.0f);
			m.ior = 1.45f;
			scene.materials[MAT_GYRO_GLASS] = m;
		}

		// Emissive Coral Pink — #E8A0A0 desaturated
		{
			Material m;
			m.type = MaterialType::Emissive;
			m.albedo = float3(0.91f, 0.63f, 0.63f);
			m.emission = float3(0.91f, 0.63f, 0.63f);
			m.emissionStr = 2.0f;
			scene.materials[MAT_GYRO_EMIT_ROSE] = m;
		}

		// Emissive Pale Lavender — #C8B8D8 desaturated
		{
			Material m;
			m.type = MaterialType::Emissive;
			m.albedo = float3(0.78f, 0.72f, 0.85f);
			m.emission = float3(0.78f, 0.72f, 0.85f);
			m.emissionStr = 2.0f;
			scene.materials[MAT_GYRO_EMIT_LAV] = m;
		}

		// Emissive Muted Teal — #8CB8B0 desaturated
		{
			Material m;
			m.type = MaterialType::Emissive;
			m.albedo = float3(0.55f, 0.72f, 0.69f);
			m.emission = float3(0.55f, 0.72f, 0.69f);
			m.emissionStr = 2.0f;
			scene.materials[MAT_GYRO_EMIT_TEAL] = m;
		}

		// Center sphere — deep Signal Red emissive
		// Reinhard x/(1+x) compresses channels non-linearly:
		// (1.0, 0.008, 0.003)*10 → HDR(10, 0.08, 0.03) → LDR(0.91, 0.07, 0.03) = deep red
		// Any more green and Reinhard turns it orange!
		{
			Material m;
			m.type = MaterialType::Emissive;
			m.albedo = float3(1.0f, 0.008f, 0.003f);
			m.emission = float3(1.0f, 0.008f, 0.003f);
			m.emissionStr = 10.0f;
			scene.materials[MAT_GYRO_CENTER] = m;
		}
	}


	// ── Rebuild sphere positions for current time ─────────────

	inline void RebuildGyroscopeSpheres(
		Tmpl8::Scene& scene,
		GyroState& state)
	{
		scene.spheres.clear();

		const float3 center(0.5f, 0.5f, 0.5f);
		int sphereIdx = 0;

		for (int r = 0; r < GyroState::NUM_RINGS; r++)
		{
			const auto& ring = state.rings[r];
			const float tilt = ring.tilt;
			const float cosTilt = cosf(tilt);
			const float sinTilt = sinf(tilt);

			for (int i = 0; i < ring.count; i++)
			{
				// Angle around the ring
				float angle = (float)i / (float)ring.count * 2.0f * PI
					+ state.time * ring.speed * state.globalSpeed;

				// Position on a circle in the XZ plane
				float x = cosf(angle) * ring.radius;
				float z = sinf(angle) * ring.radius;
				float y = 0.0f;

				// Tilt the ring: rotate (y,z) around X axis
				float ty = y * cosTilt - z * sinTilt;
				float tz = y * sinTilt + z * cosTilt;

				float3 pos = center + float3(x, ty, tz);

				uint mat = state.sphereMaterials[sphereIdx];

				scene.spheres.push_back({
					pos,
					state.orbitSphereRadius,
					mat
					});

				sphereIdx++;
			}
		}

		// Center sphere — the warning
		scene.spheres.push_back({
			center,
			state.centerSphereRadius,
			MAT_GYRO_CENTER
			});

		scene.BuildSphereBVH();
	}


	// ============================================================
	// Scene definition
	// ============================================================

	inline SceneDef GyroscopeShowcase()
	{
		SceneDef s;
		s.name = "Gyroscope";

		// ── Camera — pulled far back, looking at center ──────
		s.camPos = float3(0.5f, 0.55f, -0.15f);
		s.camTarget = float3(0.5f, 0.48f, 0.50f);

		// ── Sky — overcast white, barely any sun ──────────────
		// Clinical, shadowless, institutional
		s.sky.sunDir = normalize(float3(0.2f, -0.8f, 0.1f));
		s.sky.sunColor = float3(0.95f, 0.93f, 0.90f);
		s.sky.sunIntensity = 0.4f;
		s.sky.timeOfDay = 0.30f;
		s.sky.animate = false;

		// ── Display platform (static, baked into grid) ────────
		s.voxObjects.push_back({
			"assets/Showcase/display_platform.vox",
			float3(256, 120, 256),
			float3(0, 0, 0),
			float3(1, 1, 1), true
			});

		// ── Lights — 2 only for 60fps budget ─────────────────
		// Each point light = 1 shadow ray per voxel hit.
		// 6 lights killed the budget; 2 keeps clinical look.
		auto addLight = [&](float3 pos, float3 col)
			{
				PointLight l;
				l.position = pos;
				l.color = col;
				l.enabled = true;
				s.pointLights.push_back(l);
			};

		// Overhead — dimmed clinical wash, cool-toned
		addLight(float3(0.5f, 0.90f, 0.5f), float3(0.55f, 0.55f, 0.60f));
		// Front fill — subtle, prevents total silhouette
		addLight(float3(0.5f, 0.50f, 0.05f), float3(0.25f, 0.24f, 0.26f));
		// Center warning — pure deep red, the dominant colour in the scene
		addLight(float3(0.5f, 0.5f, 0.5f), float3(2.0f, 0.03f, 0.01f));

		// ── Shared state ──────────────────────────────────────
		auto gyro = std::make_shared<GyroState>();

		// ── Tick callback — animate every frame ───────────────
		s.tickCallback = [gyro](SceneDef& def, Tmpl8::Scene& scene,
			float deltaTime, std::function<void()> resetAcc)
			{
				if (!gyro->initialized)
				{
					gyro->initialized = true;
					SetupGyroscopeMaterials(scene);
					gyro->AssignMaterials();
					RebuildGyroscopeSpheres(scene, *gyro);
					if (resetAcc) resetAcc();
					return;
				}

				float dt = deltaTime * 0.001f;
				gyro->time += dt;

				// ── Flicker the center sphere's emission ──────────
				// Slow sinusoidal pulse: 0.8s period
				gyro->flickerPhase += dt;
				float pulse = 0.4f + 0.6f * powf(
					0.5f * (1.0f + sinf(gyro->flickerPhase * 2.0f * PI / 0.8f)), 2.0f);

				Material& centerMat = scene.materials[MAT_GYRO_CENTER];
				centerMat.emissionStr = 6.0f + 8.0f * pulse;   // 6–14 range
				// Reinhard makes even small green/blue look orange at high intensity,
				// so keep them extremely low. At peak (str=14):
				// HDR(14, 0.14, 0.06) → LDR(0.93, 0.12, 0.05) = deep red
				float r = 1.00f;
				float g = 0.006f + 0.004f * pulse;
				float b = 0.002f + 0.002f * pulse;
				centerMat.emission = float3(r, g, b);
				centerMat.albedo = float3(r, g, b);

				// Note: the center point light stays constant (red wash on platform).
				// The emissive sphere itself flickers visually via emissionStr.
				// fastSphereShading means orbit spheres skip lights anyway.

				// ── Rebuild all sphere positions ──────────────────
				RebuildGyroscopeSpheres(scene, *gyro);
				if (resetAcc) resetAcc();
			};

		// ── UI callback ───────────────────────────────────────
		s.uiCallback = [gyro](SceneDef& def, Tmpl8::Scene& scene,
			std::function<void()> resetAcc)
			{
				if (!ImGui::CollapsingHeader("Gyroscope", ImGuiTreeNodeFlags_DefaultOpen))
					return;

				ImGui::Text("%d spheres  |  %d rings  |  t = %.1fs",
					(int)scene.spheres.size(), GyroState::NUM_RINGS, gyro->time);

				ImGui::Spacing();

				// ── Color legend ──────────────────────────────────
				auto dot = [](float3 c, const char* label)
					{
						ImGui::ColorButton(label,
							ImVec4(c.x, c.y, c.z, 1.0f),
							ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker,
							ImVec2(10, 10));
						ImGui::SameLine();
						ImGui::TextDisabled("%s", label);
					};
				dot(float3(0.97f, 0.97f, 0.96f), "White (80%)");
				ImGui::SameLine();
				dot(float3(0.92f, 0.92f, 0.94f), "Chrome (10%)");
				ImGui::SameLine();
				dot(float3(0.78f, 0.18f, 0.15f), "Warning");

				ImGui::Spacing();

				bool changed = false;
				changed |= ImGui::SliderFloat("Global Speed", &gyro->globalSpeed, 0.0f, 3.0f, "%.2f");

				ImGui::Spacing();

				if (ImGui::TreeNode("Ring Parameters"))
				{
					const char* names[] = { "Ring A", "Ring B", "Ring C", "Ring D", "Ring E" };
					for (int i = 0; i < GyroState::NUM_RINGS; i++)
					{
						ImGui::PushID(i);
						if (ImGui::TreeNode(names[i]))
						{
							changed |= ImGui::SliderFloat("Tilt (rad)", &gyro->rings[i].tilt, 0.0f, PI, "%.2f");
							changed |= ImGui::SliderFloat("Speed", &gyro->rings[i].speed, -1.0f, 1.0f, "%.2f");
							changed |= ImGui::SliderFloat("Radius", &gyro->rings[i].radius, 0.10f, 0.45f, "%.3f");
							ImGui::TreePop();
						}
						ImGui::PopID();
					}
					ImGui::TreePop();
				}

				if (ImGui::TreeNode("Sphere Size"))
				{
					changed |= ImGui::SliderFloat("Orbit Radius", &gyro->orbitSphereRadius, 0.002f, 0.02f, "%.3f");
					changed |= ImGui::SliderFloat("Center Radius", &gyro->centerSphereRadius, 0.01f, 0.06f, "%.3f");
					ImGui::TreePop();
				}

				if (changed && resetAcc) resetAcc();
			};

		return s;
	}

} // namespace GameScenes