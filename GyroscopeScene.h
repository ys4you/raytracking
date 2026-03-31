
#pragma once
#include "SceneManager.h"

namespace GameScenes
{

	static constexpr uint MAT_GYRO_WHITE = MAT_RANDOM_START;
	static constexpr uint MAT_GYRO_CHROME = MAT_RANDOM_START + 1;
	static constexpr uint MAT_GYRO_GLASS = MAT_RANDOM_START + 2;
	static constexpr uint MAT_GYRO_EMIT_ROSE = MAT_RANDOM_START + 3;
	static constexpr uint MAT_GYRO_EMIT_LAV = MAT_RANDOM_START + 4;
	static constexpr uint MAT_GYRO_EMIT_TEAL = MAT_RANDOM_START + 5;
	static constexpr uint MAT_GYRO_CENTER = MAT_RANDOM_START + 6;


	enum GyroPhase
	{
		GYRO_ORDER = 0,
		GYRO_CONVERGE,
		GYRO_LOCK,
		GYRO_SPINOUT,
		GYRO_FREEZE,
		GYRO_COLLAPSE,
	};

	static constexpr float T_CONVERGE = 10.0f;
	static constexpr float T_LOCK = 14.0f;
	static constexpr float T_SPINOUT = 18.0f;
	static constexpr float T_FREEZE = 21.0f;
	static constexpr float T_COLLAPSE = 23.0f;
	static constexpr float T_RESET = 31.0f;



	struct GyroState
	{
		struct Ring
		{
			float tiltX;
			float tiltZ;
			float speed;
			int   count;
			float radius;
			int   gimbal;
		};

		static constexpr int NUM_RINGS = 6;
		static constexpr int MAX_SPHERES = 6 * 167 + 1;

		Ring rings[NUM_RINGS] = {
			{  0.00f,       0.00f,       0.30f,  167, 0.42f, 0 },
			{  0.00f,       0.00f,      -0.30f,  167, 0.38f, 0 },
			{  PI * 0.5f,   0.00f,       0.25f,  167, 0.36f, 1 },
			{  PI * 0.5f,   0.00f,      -0.25f,  167, 0.32f, 1 },
			{  0.00f,       PI * 0.5f,   0.35f,  167, 0.35f, 2 },
			{  0.00f,       PI * 0.5f,  -0.35f,  167, 0.31f, 2 },
		};

		float gimbalPrecession[3] = { 0.12f, 0.10f, 0.14f };

		float time = 0.0f;
		float cycleTime = 0.0f;
		bool  initialized = false;
		float flickerPhase = 0.0f;
		GyroPhase phase = GYRO_ORDER;

		float orbitSphereRadius = 0.005f;
		float centerSphereRadius = 0.14f;

		float globalSpeed = 1.0f;
		float precessionSpeed = 1.0f;

		std::vector<float3> chaosVelocity;
		std::vector<float3> chaosOffset;
		bool chaosInitialized = false;
		float collapseTime = 0.0f;

		float animOrbitMul = 1.0f;
		float animPrecessMul = 1.0f;
		float animLockBlend = 0.0f;
		float animPanicWobble = 0.0f;
		float animRadiusDecay = 1.0f;

		std::vector<uint> sphereMaterials;

		void AssignMaterials()
		{
			sphereMaterials.clear();
			int total = 0;
			for (int i = 0; i < NUM_RINGS; i++) total += rings[i].count;
			sphereMaterials.reserve(total);

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

		void InitChaos()
		{
			if (chaosInitialized) return;
			int total = 0;
			for (int i = 0; i < NUM_RINGS; i++) total += rings[i].count;
			total++;

			chaosVelocity.resize(total);
			chaosOffset.resize(total);
			uint seed = 1234;
			for (int i = 0; i < total; i++)
			{
				chaosVelocity[i] = float3(
					(RandomFloat(seed) - 0.5f) * 0.15f,
					(RandomFloat(seed) - 0.5f) * 0.15f,
					(RandomFloat(seed) - 0.5f) * 0.15f
				);
				chaosVelocity[i].y -= 0.03f;
				chaosOffset[i] = float3(0, 0, 0);
			}
			collapseTime = 0.0f;
			chaosInitialized = true;
		}

		void ResetChaos()
		{
			chaosOffset.clear();
			chaosVelocity.clear();
			chaosInitialized = false;
			collapseTime = 0.0f;
		}

		void ResetCycle()
		{
			cycleTime = 0.0f;
			phase = GYRO_ORDER;
			animOrbitMul = 1.0f;
			animPrecessMul = 1.0f;
			animLockBlend = 0.0f;
			animPanicWobble = 0.0f;
			animRadiusDecay = 1.0f;
			ResetChaos();
		}
	};



	inline void SetupGyroscopeMaterials(Tmpl8::Scene& scene)
	{
		{
			Material m; m.type = MaterialType::Lambertian;
			m.albedo = float3(0.973f, 0.969f, 0.957f); m.roughness = 0.35f;
			scene.materials[MAT_GYRO_WHITE] = m;
		}
		{
			Material m; m.type = MaterialType::Metal;
			m.albedo = float3(0.92f, 0.92f, 0.94f); m.roughness = 0.02f; m.metallic = 1.0f;
			scene.materials[MAT_GYRO_CHROME] = m;
		}
		{
			Material m; m.type = MaterialType::Dielectric;
			m.albedo = float3(0.98f, 0.98f, 1.0f); m.ior = 1.45f;
			scene.materials[MAT_GYRO_GLASS] = m;
		}
		{
			Material m; m.type = MaterialType::Emissive;
			m.albedo = float3(0.91f, 0.63f, 0.63f); m.emission = m.albedo; m.emissionStr = 2.0f;
			scene.materials[MAT_GYRO_EMIT_ROSE] = m;
		}
		{
			Material m; m.type = MaterialType::Emissive;
			m.albedo = float3(0.78f, 0.72f, 0.85f); m.emission = m.albedo; m.emissionStr = 2.0f;
			scene.materials[MAT_GYRO_EMIT_LAV] = m;
		}
		{
			Material m; m.type = MaterialType::Emissive;
			m.albedo = float3(0.55f, 0.72f, 0.69f); m.emission = m.albedo; m.emissionStr = 2.0f;
			scene.materials[MAT_GYRO_EMIT_TEAL] = m;
		}
		{
			Material m; m.type = MaterialType::Emissive;
			m.albedo = float3(1.0f, 0.008f, 0.003f); m.emission = m.albedo; m.emissionStr = 10.0f;
			scene.materials[MAT_GYRO_CENTER] = m;
		}
	}


	inline float smoothstep(float a, float b, float t)
	{
		float x = (t - a) / (b - a);
		x = x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x);
		return x * x * (3.0f - 2.0f * x);
	}



	inline void RebuildGyroscopeSpheres(
		Tmpl8::Scene& scene,
		GyroState& state)
	{
		scene.spheres.clear();

		const float3 center(0.5f, 0.5f, 0.5f);
		const float t = state.time;
		int sphereIdx = 0;

		float aY = t * state.gimbalPrecession[0] * state.precessionSpeed * state.animPrecessMul;
		float aX = t * state.gimbalPrecession[1] * state.precessionSpeed * state.animPrecessMul;
		float aZ = t * state.gimbalPrecession[2] * state.precessionSpeed * state.animPrecessMul;

		if (state.animLockBlend > 0.0f)
		{
			float blend = state.animLockBlend;
			aX = aX * (1.0f - blend) + 0.0f * blend;
			aZ = aZ * (1.0f - blend) + 0.0f * blend;
		}

		if (state.animPanicWobble > 0.0f)
		{
			float w = state.animPanicWobble;
			float freq = t * 12.0f;
			aX += w * sinf(freq * 2.1f);
			aY += w * sinf(freq * 1.7f + 1.0f);
			aZ += w * sinf(freq * 3.3f + 2.0f);
		}

		const float cyG = cosf(aY), syG = sinf(aY);
		const float cxG = cosf(aX), sxG = sinf(aX);
		const float czG = cosf(aZ), szG = sinf(aZ);

		for (int r = 0; r < GyroState::NUM_RINGS; r++)
		{
			const auto& ring = state.rings[r];
			const int g = ring.gimbal;
			const float radius = ring.radius * state.animRadiusDecay;

			float bTiltX = ring.tiltX;
			float bTiltZ = ring.tiltZ;
			if (state.animLockBlend > 0.0f)
			{
				bTiltX = ring.tiltX * (1.0f - state.animLockBlend);
				bTiltZ = ring.tiltZ * (1.0f - state.animLockBlend);
			}

			const float cx = cosf(bTiltX), sx = sinf(bTiltX);
			const float cz = cosf(bTiltZ), sz = sinf(bTiltZ);

			for (int i = 0; i < ring.count; i++)
			{
				float angle = (float)i / (float)ring.count * 2.0f * PI
					+ t * ring.speed * state.globalSpeed * state.animOrbitMul;

				float px = cosf(angle) * radius;
				float py = 0.0f;
				float pz = sinf(angle) * radius;

				float ty = py * cx - pz * sx;
				float tz = py * sx + pz * cx;
				py = ty; pz = tz;
				float tx = px * cz - py * sz;
				ty = px * sz + py * cz;
				px = tx; py = ty;

				tx = px * cyG + pz * syG;
				tz = -px * syG + pz * cyG;
				px = tx; pz = tz;

				if (g >= 1)
				{
					ty = py * cxG - pz * sxG;
					tz = py * sxG + pz * cxG;
					py = ty; pz = tz;
				}
				if (g >= 2)
				{
					tx = px * czG - py * szG;
					ty = px * szG + py * czG;
					px = tx; py = ty;
				}

				float3 pos = center + float3(px, py, pz);

				if (state.chaosInitialized && sphereIdx < (int)state.chaosOffset.size())
					pos += state.chaosOffset[sphereIdx];

				scene.spheres.push_back({
					pos,
					state.orbitSphereRadius,
					state.sphereMaterials[sphereIdx]
					});
				sphereIdx++;
			}
		}

		float3 centerPos = center;
		if (state.chaosInitialized && sphereIdx < (int)state.chaosOffset.size())
			centerPos += state.chaosOffset[sphereIdx];

		scene.spheres.push_back({
			centerPos,
			state.centerSphereRadius,
			MAT_GYRO_CENTER
			});

		scene.BuildSphereBVH();
	}



	inline SceneDef GyroscopeShowcase()
	{
		SceneDef s;
		s.name = "Gyroscope";
		s.useVoxelGrid = false;

		s.camPos = float3(0.5f, 0.46f, 0.25f);
		s.camTarget = float3(0.5f, 0.45f, 0.5f);

		const float3 c(0.5f, 0.5f, 0.5f);
		s.splinePoints = {
			c + float3(0.00f,  0.15f, -0.85f),
			c + float3(0.70f, -0.10f, -0.50f),
			c + float3(0.85f, -0.35f,  0.00f),
			c + float3(0.30f, -0.40f,  0.80f),
			c + float3(-0.20f, 0.20f,  0.85f),
			c + float3(-0.15f, 0.90f,  0.20f),
			c + float3(-0.80f, 0.10f, -0.30f),
			c + float3(-0.40f,-0.05f, -0.50f),
			c + float3(0.00f,  0.15f, -0.85f),
		};

		s.sky.sunDir = normalize(float3(0.2f, -0.8f, 0.1f));
		s.sky.sunColor = float3(0.95f, 0.93f, 0.90f);
		s.sky.sunIntensity = 0.4f;
		s.sky.timeOfDay = 0.30f;
		s.sky.animate = false;

		auto addLight = [&](float3 pos, float3 col)
			{
				PointLight l;
				l.position = pos; l.color = col; l.enabled = true;
				s.pointLights.push_back(l);
			};
		addLight(float3(0.5f, 0.90f, 0.5f), float3(0.55f, 0.55f, 0.60f));
		addLight(float3(0.5f, 0.50f, 0.05f), float3(0.25f, 0.24f, 0.26f));
		addLight(float3(0.5f, 0.5f, 0.5f), float3(2.0f, 0.03f, 0.01f));

		auto gyro = std::make_shared<GyroState>();

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
				gyro->cycleTime += dt;
				float ct = gyro->cycleTime;

				if (ct >= T_RESET)
				{
					gyro->ResetCycle();
					ct = 0.0f;
				}

				if (ct < T_CONVERGE)
				{
					gyro->phase = GYRO_ORDER;
					gyro->animOrbitMul = 1.5f;
					gyro->animPrecessMul = 1.0f;
					gyro->animLockBlend = 0.0f;
					gyro->animPanicWobble = 0.0f;
					gyro->animRadiusDecay = 1.0f;
				}
				else if (ct < T_LOCK)
				{
					gyro->phase = GYRO_CONVERGE;
					float p = (ct - T_CONVERGE) / (T_LOCK - T_CONVERGE);
					gyro->animLockBlend = smoothstep(0.0f, 1.0f, p);
					gyro->animOrbitMul = 1.5f + 0.5f * p;
					gyro->animPrecessMul = 1.0f + 0.5f * p;
				}
				else if (ct < T_SPINOUT)
				{
					gyro->phase = GYRO_LOCK;
					float p = (ct - T_LOCK) / (T_SPINOUT - T_LOCK);
					gyro->animLockBlend = 1.0f;
					gyro->animPanicWobble = 0.3f + 0.7f * p;
					gyro->animOrbitMul = 2.0f + 3.0f * p;
					gyro->animPrecessMul = 1.5f + 2.0f * p;
				}
				else if (ct < T_FREEZE)
				{
					gyro->phase = GYRO_SPINOUT;
					float p = (ct - T_SPINOUT) / (T_FREEZE - T_SPINOUT);
					gyro->animLockBlend = 1.0f - 0.3f * p;
					gyro->animPanicWobble = 1.0f + 1.5f * p;
					gyro->animOrbitMul = 5.0f + 10.0f * p;
					gyro->animPrecessMul = 3.5f + 5.0f * p;
				}
				else if (ct < T_COLLAPSE)
				{
					gyro->phase = GYRO_FREEZE;
					float p = (ct - T_FREEZE) / (T_COLLAPSE - T_FREEZE);
					float brake = 1.0f - smoothstep(0.0f, 1.0f, p);
					gyro->animOrbitMul = 15.0f * brake;
					gyro->animPrecessMul = 8.5f * brake;
					gyro->animPanicWobble = 2.5f * brake;
					gyro->animLockBlend = 0.7f * brake;
				}
				else
				{
					gyro->phase = GYRO_COLLAPSE;
					gyro->animOrbitMul = 0.0f;
					gyro->animPrecessMul = 0.0f;
					gyro->animPanicWobble = 0.0f;
					gyro->animLockBlend = 0.0f;

					gyro->InitChaos();
					gyro->collapseTime += dt;

					float chaos_t = gyro->collapseTime;
					float accel = 1.0f + chaos_t * 0.5f;
					for (int i = 0; i < (int)gyro->chaosOffset.size(); i++)
					{
						gyro->chaosOffset[i] += gyro->chaosVelocity[i] * dt * accel;
						gyro->chaosVelocity[i].y -= 0.02f * dt;
					}

					gyro->animRadiusDecay = 1.0f + chaos_t * 0.3f;
				}

				gyro->flickerPhase += dt;
				float basePulse = 0.4f + 0.6f * powf(
					0.5f * (1.0f + sinf(gyro->flickerPhase * 2.0f * PI / 0.8f)), 2.0f);

				float flickerMul = 1.0f;
				if (gyro->phase == GYRO_LOCK || gyro->phase == GYRO_SPINOUT)
				{
					float erratic = sinf(gyro->time * 30.0f) * sinf(gyro->time * 47.0f);
					flickerMul = 1.0f + 1.5f * fabsf(erratic);
				}
				else if (gyro->phase == GYRO_FREEZE)
				{
					flickerMul = 0.3f;
				}
				else if (gyro->phase == GYRO_COLLAPSE)
				{
					float fade = 1.0f - gyro->collapseTime / (T_RESET - T_COLLAPSE);
					flickerMul = max(0.0f, fade * 0.5f);
				}

				Material& centerMat = scene.materials[MAT_GYRO_CENTER];
				centerMat.emissionStr = (6.0f + 8.0f * basePulse) * flickerMul;
				float r = 1.00f;
				float g = 0.006f + 0.004f * basePulse;
				float b = 0.002f + 0.002f * basePulse;
				centerMat.emission = float3(r, g, b);
				centerMat.albedo = float3(r, g, b);

				RebuildGyroscopeSpheres(scene, *gyro);
				if (resetAcc) resetAcc();
			};

		s.uiCallback = [gyro](SceneDef& def, Tmpl8::Scene& scene,
			std::function<void()> resetAcc)
			{
				if (!ImGui::CollapsingHeader("Gyroscope", ImGuiTreeNodeFlags_DefaultOpen))
					return;

				static const char* phaseNames[] = {
					"ORDER", "CONVERGE", "GIMBAL LOCK", "SPIN OUT", "FREEZE", "COLLAPSE"
				};
				ImVec4 phaseColor = ImVec4(0.3f, 0.8f, 0.3f, 1.0f);
				if (gyro->phase == GYRO_CONVERGE) phaseColor = ImVec4(0.9f, 0.7f, 0.2f, 1.0f);
				if (gyro->phase == GYRO_LOCK)     phaseColor = ImVec4(0.9f, 0.3f, 0.1f, 1.0f);
				if (gyro->phase == GYRO_SPINOUT)  phaseColor = ImVec4(1.0f, 0.1f, 0.1f, 1.0f);
				if (gyro->phase == GYRO_FREEZE)   phaseColor = ImVec4(0.5f, 0.5f, 0.8f, 1.0f);
				if (gyro->phase == GYRO_COLLAPSE) phaseColor = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);

				ImGui::TextColored(phaseColor, "Phase: %s", phaseNames[gyro->phase]);
				ImGui::SameLine();
				ImGui::TextDisabled("%.1f / %.0fs", gyro->cycleTime, T_RESET);

				ImGui::ProgressBar(gyro->cycleTime / T_RESET, ImVec2(-1, 3));

				ImGui::Text("%d spheres  |  cycle t = %.1fs",
					(int)scene.spheres.size(), gyro->cycleTime);

				ImGui::Spacing();

				bool changed = false;
				changed |= ImGui::SliderFloat("Orbit Speed", &gyro->globalSpeed, 0.0f, 3.0f, "%.2f");
				changed |= ImGui::SliderFloat("Precession Speed", &gyro->precessionSpeed, 0.0f, 3.0f, "%.2f");

				if (ImGui::Button("Reset Cycle"))
				{
					gyro->ResetCycle();
					changed = true;
				}

				ImGui::Spacing();

				if (ImGui::TreeNode("Animation State"))
				{
					ImGui::TextDisabled("Orbit mul:    %.2f", gyro->animOrbitMul);
					ImGui::TextDisabled("Precess mul:  %.2f", gyro->animPrecessMul);
					ImGui::TextDisabled("Lock blend:   %.2f", gyro->animLockBlend);
					ImGui::TextDisabled("Panic wobble: %.2f", gyro->animPanicWobble);
					ImGui::TextDisabled("Radius decay: %.2f", gyro->animRadiusDecay);
					ImGui::TreePop();
				}

				if (ImGui::TreeNode("Gimbal Precession"))
				{
					changed |= ImGui::SliderFloat("Outer (Y)", &gyro->gimbalPrecession[0], -0.5f, 0.5f, "%.3f");
					changed |= ImGui::SliderFloat("Middle (X)", &gyro->gimbalPrecession[1], -0.5f, 0.5f, "%.3f");
					changed |= ImGui::SliderFloat("Inner (Z)", &gyro->gimbalPrecession[2], -0.5f, 0.5f, "%.3f");
					ImGui::TreePop();
				}

				if (ImGui::TreeNode("Ring Parameters"))
				{
					const char* names[] = { "XZ-A", "XZ-B", "XY-A", "XY-B", "YZ-A", "YZ-B" };
					for (int i = 0; i < GyroState::NUM_RINGS; i++)
					{
						ImGui::PushID(i);
						if (ImGui::TreeNode(names[i]))
						{
							changed |= ImGui::SliderFloat("Tilt X", &gyro->rings[i].tiltX, 0.0f, PI * 2, "%.2f");
							changed |= ImGui::SliderFloat("Tilt Z", &gyro->rings[i].tiltZ, 0.0f, PI * 2, "%.2f");
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
					changed |= ImGui::SliderFloat("Orbit", &gyro->orbitSphereRadius, 0.002f, 0.02f, "%.3f");
					changed |= ImGui::SliderFloat("Center", &gyro->centerSphereRadius, 0.01f, 0.20f, "%.3f");
					ImGui::TreePop();
				}

				if (changed && resetAcc) resetAcc();
			};

		return s;
	}

}