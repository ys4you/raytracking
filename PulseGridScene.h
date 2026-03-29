// ============================================================
// PulseGridScene.h — Beat-Synced Pulse Wave Grid
// ============================================================
// An 8×8 grid of cubes that reacts to 79 BPM rhythm.
// Each beat spawns a circular wave from a random position.
// Cubes rise in height and glow teal as the wave passes.
// Multiple waves overlap creating interference patterns.
//
// Like ripples on a sterile white lake — chaotic but rhythmic.
//
// Side Order palette:
//   Near White   #F5F5F5  — resting cubes
//   Muted Teal   #8CB8B0  — active/pulsed cubes
//   Emissive flash        — peak pulse moment
//
// 79 BPM, cubic ease-out.
// ============================================================

#pragma once
#include "SceneManager.h"
#include "VoxelFactory.h"
#include "VoxelObject.h"
#include <vector>
#include <cstring>
#include <algorithm>
#include <cmath>

namespace GameScenes
{

	static constexpr uint8_t PAL_PG_REST  = 235;  // Near White
	static constexpr uint8_t PAL_PG_PULSE = 236;  // Muted Teal
	static constexpr uint8_t PAL_PG_FLASH = 237;  // Emissive peak

	static constexpr float PG_BPM  = 79.0f;
	static constexpr float PG_BEAT = 60.0f / PG_BPM;
	static constexpr float PG_RAD2DEG = 180.0f / 3.14159265f;

	static constexpr int PG_GRID = 8;        // 8×8 grid
	static constexpr int PG_MAX_WAVES = 6;   // max simultaneous waves


	struct PulseWave
	{
		float ox, oz;       // origin on grid (0..7)
		float radius;       // current radius (expands over time)
		float life;         // 0..1, fades out
		bool  active;
	};


	struct PulseGridState
	{
		PulseWave waves[PG_MAX_WAVES] = {};
		int nextWave = 0;

		float beatTimer = 0.0f;
		float beatPhase = 0.0f;    // 0..1 within current beat
		int   beatCount = 0;

		// Wave parameters
		float waveSpeed = 12.0f;    // grid cells per second
		float waveDecay = 1.8f;     // seconds to full decay
		float maxHeight = 25.0f;    // max Y displacement
		float waveWidth = 1.5f;     // ring width in cells

		// Rotation
		float angleX = 0, angleY = 0, angleZ = 0;
		float speedX = 0.03f, speedY = 0.10f, speedZ = 0.02f;

		bool  paused = false;
		float speedMul = 1.0f;

		int  objBase = -1;
		bool needsInit = true;

		// Bloom spike
		float bloomSpike = 0.0f;
		float bloomDecay = 10.0f;

		void SpawnWave()
		{
			PulseWave& w = waves[nextWave % PG_MAX_WAVES];
			w.ox = (float)(rand() % PG_GRID);
			w.oz = (float)(rand() % PG_GRID);
			w.radius = 0.0f;
			w.life = 1.0f;
			w.active = true;
			nextWave++;

			bloomSpike = 0.4f;
		}

		// Get combined wave amplitude at grid position
		float GetAmplitude(int gx, int gz) const
		{
			float amp = 0.0f;
			for (int i = 0; i < PG_MAX_WAVES; i++)
			{
				if (!waves[i].active) continue;
				const PulseWave& w = waves[i];

				float dx = (float)gx - w.ox;
				float dz = (float)gz - w.oz;
				float dist = sqrtf(dx * dx + dz * dz);

				// Ring shape: peak at radius, falls off on both sides
				float ringDist = fabsf(dist - w.radius);
				float ring = expf(-(ringDist * ringDist) / (waveWidth * waveWidth));

				amp += ring * w.life;
			}
			return std::clamp(amp, 0.0f, 1.0f);
		}

		void TickRotation(float dtMs)
		{
			float dt = dtMs * 0.001f;
			angleX += speedX * dt;
			angleY += speedY * dt;
			angleZ += speedZ * dt;
			if (angleX > 2 * PI) angleX -= 2 * PI;
			if (angleY > 2 * PI) angleY -= 2 * PI;
			if (angleZ > 2 * PI) angleZ -= 2 * PI;

			if (bloomSpike > 0.01f)
				bloomSpike *= expf(-bloomDecay * dt);
			else
				bloomSpike = 0.0f;
		}

		void Tick(float dtMs)
		{
			if (paused) return;
			float dt = dtMs * 0.001f * speedMul;

			// Beat tracking
			beatTimer += dt;
			beatPhase = fmodf(beatTimer / PG_BEAT, 1.0f);

			// Spawn wave on each beat
			int currentBeat = (int)(beatTimer / PG_BEAT);
			if (currentBeat > beatCount)
			{
				beatCount = currentBeat;
				SpawnWave();
			}

			// Update waves
			for (int i = 0; i < PG_MAX_WAVES; i++)
			{
				if (!waves[i].active) continue;
				waves[i].radius += waveSpeed * dt;
				waves[i].life -= dt / waveDecay;
				if (waves[i].life <= 0.0f)
					waves[i].active = false;
			}
		}

		float3 GetRot() const { return float3(angleX, angleY, angleZ); }

		void Reset()
		{
			beatTimer = 0;
			beatPhase = 0;
			beatCount = 0;
			bloomSpike = 0;
			for (int i = 0; i < PG_MAX_WAVES; i++)
				waves[i].active = false;
			nextWave = 0;
		}
	};


	inline std::shared_ptr<PulseGridState>& GetPulseGridState()
	{
		static std::shared_ptr<PulseGridState> inst;
		return inst;
	}


	inline void SetupPulseGridCubes(Tmpl8::Scene& scene, PulseGridState& pg)
	{
		pg.objBase = (int)scene.voxelObjects.size();

		scene.voxelObjects.push_back(VoxelObject(1, 1, 1, { PAL_PG_REST }));   // 0: rest
		scene.voxelObjects.push_back(VoxelObject(1, 1, 1, { PAL_PG_PULSE }));  // 1: pulse
		scene.voxelObjects.push_back(VoxelObject(1, 1, 1, { PAL_PG_FLASH }));  // 2: flash

		auto mat = [](float3 c, float r) -> Material
			{
				Material m;
				m.type = MaterialType::Lambertian;
				m.albedo = c;
				m.roughness = r;
				return m;
			};

		// Near White — resting
		scene.materials[MAT_COUNT + (PAL_PG_REST - 1)] = mat(float3(0.96f, 0.96f, 0.96f), 0.30f);
		// Muted Teal — pulsed
		scene.materials[MAT_COUNT + (PAL_PG_PULSE - 1)] = mat(float3(0.549f, 0.722f, 0.690f), 0.20f);
		// Emissive flash — peak
		{
			Material m;
			m.type = MaterialType::Emissive;
			m.albedo = float3(0.7f, 0.9f, 0.85f);
			m.emission = float3(0.549f, 0.722f, 0.690f);
			m.emissionStr = 4.0f;
			m.roughness = 0.1f;
			scene.materials[MAT_COUNT + (PAL_PG_FLASH - 1)] = m;
		}
	}


	inline void SyncPulseGrid(
		const PulseGridState& pg,
		Tmpl8::Scene& scene,
		float3 rotRad)
	{
		scene.voxelInstances.clear();

		const float gridExt = 80.0f;
		const float cellSize = gridExt / (float)PG_GRID;
		const float baseScale = cellSize * 0.92f;
		const float3 center(256, 240, 256);

		float3 rotDeg = rotRad * PG_RAD2DEG;

		mat4 rot = mat4::RotateX(rotRad.x)
			* mat4::RotateY(rotRad.y)
			* mat4::RotateZ(rotRad.z);

		auto orbit = [&](float3 p) -> float3
			{
				float3 o = p - center;
				float4 r = rot * float4(o, 1);
				return float3(r.x, r.y, r.z) + center;
			};

		for (int gx = 0; gx < PG_GRID; gx++)
			for (int gz = 0; gz < PG_GRID; gz++)
			{
				float amp = pg.GetAmplitude(gx, gz);

				// Position: flat grid, Y rises with amplitude
				float3 pos(
					center.x - gridExt * 0.5f + (gx + 0.5f) * cellSize,
					center.y + amp * pg.maxHeight,
					center.z - gridExt * 0.5f + (gz + 0.5f) * cellSize
				);

				// Scale: base width, height stretches with amplitude
				float heightScale = baseScale * (1.0f + amp * 2.0f);

				// Color: rest → pulse → flash based on amplitude
				int objIdx;
				if (amp > 0.7f)
					objIdx = pg.objBase + 2;  // emissive flash
				else if (amp > 0.15f)
					objIdx = pg.objBase + 1;  // muted teal
				else
					objIdx = pg.objBase + 0;  // near white

				VoxelFactory::CreateInstance(
					scene, objIdx,
					orbit(pos), rotDeg,
					float3(baseScale, heightScale, baseScale)
				);
			}

		scene.RebuildDirtyInstances();
	}


	// ── Scene ─────────────────────────────────────────────────────

	inline SceneDef PulseGridShowcase()
	{
		SceneDef s;
		s.name = "Pulse Grid";
		s.useVoxelGrid = true;

		s.voxObjects.push_back({
			"assets/Showcase/display_platform.vox",
			float3(256, 120, 256), float3(0), float3(1), true
			});

		s.camPos = float3(0.28f, 0.60f, 0.20f);
		s.camTarget = float3(0.50f, 0.47f, 0.50f);

		// Orbit spline
		s.splinePoints = {
			float3(0.25f, 0.60f, 0.22f),
			float3(0.50f, 0.60f, 0.15f),
			float3(0.75f, 0.60f, 0.22f),
			float3(0.78f, 0.60f, 0.50f),
			float3(0.75f, 0.60f, 0.78f),
			float3(0.50f, 0.60f, 0.85f),
			float3(0.25f, 0.60f, 0.78f),
			float3(0.22f, 0.60f, 0.50f),
			float3(0.25f, 0.60f, 0.22f),
		};

		// Clinical lighting
		s.sky.sunDir = normalize(float3(0.3f, -0.5f, 0.2f));
		s.sky.sunColor = float3(1.0f, 0.98f, 0.95f);
		s.sky.sunIntensity = 2.0f;
		s.sky.timeOfDay = 0.35f;
		s.sky.animate = false;

		auto addL = [&](float3 p, float3 c)
			{
				PointLight l;
				l.position = p; l.color = c; l.enabled = true;
				s.pointLights.push_back(l);
			};
		addL(float3(0.50f, 0.90f, 0.50f), float3(0.9f, 0.88f, 0.85f));
		addL(float3(0.25f, 0.55f, 0.20f), float3(0.4f, 0.4f, 0.45f));
		addL(float3(0.75f, 0.55f, 0.80f), float3(0.4f, 0.4f, 0.45f));
		addL(float3(0.50f, 0.35f, 0.25f), float3(0.25f, 0.25f, 0.28f));

		auto& pg = GetPulseGridState();
		pg = std::make_shared<PulseGridState>();

		s.tickCallback = [pg](SceneDef&, Tmpl8::Scene& scene,
			float dt, std::function<void()> reset)
			{
				if (pg->needsInit)
				{
					pg->needsInit = false;
					SetupPulseGridCubes(scene, *pg);
					SyncPulseGrid(*pg, scene, pg->GetRot());
					if (reset) reset();
					return;
				}

				pg->TickRotation(dt);
				pg->Tick(dt);
				SyncPulseGrid(*pg, scene, pg->GetRot());
			};

		s.uiCallback = [pg](SceneDef&, Tmpl8::Scene& scene,
			std::function<void()> reset)
			{
				if (!ImGui::CollapsingHeader("Pulse Grid", ImGuiTreeNodeFlags_DefaultOpen))
					return;

				ImGui::Text("Beat: %d  |  Phase: %.0f%%  |  %d waves  |  %d inst",
					pg->beatCount,
					pg->beatPhase * 100.0f,
					[&]() { int n = 0; for (int i = 0; i < PG_MAX_WAVES; i++) if (pg->waves[i].active) n++; return n; }(),
					(int)scene.voxelInstances.size());

				// Beat indicator
				ImGui::ProgressBar(pg->beatPhase, ImVec2(-1, 3));

				ImGui::Spacing();

				auto dot = [](float3 c, const char* l)
					{
						ImGui::ColorButton(l, ImVec4(c.x, c.y, c.z, 1),
							ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker,
							ImVec2(12, 12));
						ImGui::SameLine(); ImGui::Text("%s", l);
					};
				dot(float3(0.96f), "Rest");
				ImGui::SameLine();
				dot(float3(0.549f, 0.722f, 0.690f), "Pulse");
				ImGui::SameLine();
				dot(float3(0.7f, 0.9f, 0.85f), "Peak");

				ImGui::Spacing(); ImGui::Separator();

				if (ImGui::Button(pg->paused ? "  Play  " : " Pause  "))
					pg->paused = !pg->paused;
				ImGui::SameLine();
				if (ImGui::Button("Reset"))
				{
					pg->Reset();
					SyncPulseGrid(*pg, scene, pg->GetRot());
					if (reset) reset();
				}

				ImGui::SliderFloat("Speed##pg", &pg->speedMul, 0.25f, 4.0f, "%.2fx");

				ImGui::Spacing();

				if (ImGui::CollapsingHeader("Wave Settings"))
				{
					ImGui::SliderFloat("Wave Speed##pg", &pg->waveSpeed, 4, 24, "%.1f cells/s");
					ImGui::SliderFloat("Wave Decay##pg", &pg->waveDecay, 0.5f, 4.0f, "%.1f s");
					ImGui::SliderFloat("Max Height##pg", &pg->maxHeight, 5, 50, "%.0f");
					ImGui::SliderFloat("Ring Width##pg", &pg->waveWidth, 0.5f, 4.0f, "%.1f");
				}

				if (ImGui::CollapsingHeader("Rotation"))
				{
					ImGui::SliderFloat("X##pg", &pg->speedX, 0, 0.5f, "%.2f");
					ImGui::SliderFloat("Y##pg", &pg->speedY, 0, 0.5f, "%.2f");
					ImGui::SliderFloat("Z##pg", &pg->speedZ, 0, 0.5f, "%.2f");
				}
			};

		return s;
	}

} // namespace GameScenes
