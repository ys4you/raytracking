// ============================================================
// LivingCubeScene.h — 3D Conway's Game of Life
// ============================================================
// Uses 4 colored 1×1×1 voxel cubes instanced per alive cell.
// Color encodes each cell's predicted fate:
//   Muted Teal    — stable (will survive)
//   Soft Yellow   — newly born this generation
//   Coral Pink    — dying from underpopulation
//   Pale Lavender — dying from overpopulation
//
// Colors reference Splatoon 3: Side Order palette.
// ============================================================

#pragma once
#include "SceneManager.h"
#include "VoxelFactory.h"
#include "VoxelObject.h"
#include <vector>
#include <cstring>
#include <algorithm>

namespace GameScenes
{

	// ── Cell visual state ─────────────────────────────────────────
	enum CellVis : uint8_t
	{
		VIS_STABLE = 0,  // will survive next gen
		VIS_BORN = 1,  // just born this gen
		VIS_DYING_UNDER = 2,  // will die — underpopulation
		VIS_DYING_OVER = 3   // will die — overpopulation
	};

	// Palette indices (high range to avoid conflicts with .vox palettes)
	static constexpr uint8_t PAL_STABLE = 250;
	static constexpr uint8_t PAL_BORN = 251;
	static constexpr uint8_t PAL_DYING_UNDER = 252;
	static constexpr uint8_t PAL_DYING_OVER = 253;


	// ── Automaton state ───────────────────────────────────────────
	struct LifeState
	{
		static constexpr int   MAX_DIM = 8;
		static constexpr float GRID_SIZE = 512.0f;

		int     dim = 6;
		bool    cells[MAX_DIM][MAX_DIM][MAX_DIM] = {};
		bool    buffer[MAX_DIM][MAX_DIM][MAX_DIM] = {};
		CellVis cellVis[MAX_DIM][MAX_DIM][MAX_DIM] = {};
		int     generation = 0;

		// Rules — default: "Coral" 4-6/5-6
		int surviveMin = 4, surviveMax = 6;
		int birthMin = 5, birthMax = 6;

		// Timing
		float stepInterval = 0.2f;
		float timer = 0.0f;
		bool  paused = false;
		bool  needsInitialSync = true;

		// Visual
		float cubeSpacing = 1.2f;

		// Rotation
		float angleX = 0.0f, angleY = 0.0f, angleZ = 0.0f;
		float speedX = 0.12f, speedY = 0.25f, speedZ = 0.08f;

		// Object indices — set on first tick by SetupColoredCubes
		int objBase = 0;

		void TickRotation(float deltaTimeMs)
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

		// Presets
		enum Preset { P_CORAL, P_GROWTH, P_SPRAWL, P_CUSTOM, P_COUNT };
		int preset = P_CORAL;

		void ApplyPreset()
		{
			switch (preset)
			{
			case P_CORAL:  surviveMin = 4; surviveMax = 6; birthMin = 5; birthMax = 6; break;
			case P_GROWTH: surviveMin = 5; surviveMax = 8; birthMin = 6; birthMax = 7; break;
			case P_SPRAWL: surviveMin = 4; surviveMax = 8; birthMin = 5; birthMax = 7; break;
			default: break;
			}
		}

		void Clear()
		{
			memset(cells, 0, sizeof(cells));
			memset(buffer, 0, sizeof(buffer));
			memset(cellVis, 0, sizeof(cellVis));
			generation = 0;
			timer = 0.0f;
		}

		void SeedRandom(float density = 0.35f)
		{
			Clear();
			int pad = dim / 4;
			for (int x = pad; x < dim - pad; x++)
				for (int y = pad; y < dim - pad; y++)
					for (int z = pad; z < dim - pad; z++)
						cells[x][y][z] = (RandomFloat() < density);
			ClassifyFates();
		}

		int CountNeighbors(int cx, int cy, int cz) const
		{
			int n = 0;
			for (int dx = -1; dx <= 1; dx++)
				for (int dy = -1; dy <= 1; dy++)
					for (int dz = -1; dz <= 1; dz++)
					{
						if (dx == 0 && dy == 0 && dz == 0) continue;
						int nx = cx + dx, ny = cy + dy, nz = cz + dz;
						if (nx >= 0 && nx < dim &&
							ny >= 0 && ny < dim &&
							nz >= 0 && nz < dim &&
							cells[nx][ny][nz])
							n++;
					}
			return n;
		}

		void Step()
		{
			// 1. Compute next generation in buffer
			for (int x = 0; x < dim; x++)
				for (int y = 0; y < dim; y++)
					for (int z = 0; z < dim; z++)
					{
						int nb = CountNeighbors(x, y, z);
						if (cells[x][y][z])
							buffer[x][y][z] = (nb >= surviveMin && nb <= surviveMax);
						else
							buffer[x][y][z] = (nb >= birthMin && nb <= birthMax);
					}

			// 2. Tag born cells (was dead, now alive)
			for (int x = 0; x < dim; x++)
				for (int y = 0; y < dim; y++)
					for (int z = 0; z < dim; z++)
					{
						if (!buffer[x][y][z]) continue;
						cellVis[x][y][z] = (!cells[x][y][z])
							? VIS_BORN
							: VIS_STABLE;
					}

			// 3. Apply new generation
			memcpy(cells, buffer, sizeof(cells));
			generation++;

			// 4. Predict fate for surviving cells
			//    (born cells keep their yellow color for one generation)
			for (int x = 0; x < dim; x++)
				for (int y = 0; y < dim; y++)
					for (int z = 0; z < dim; z++)
					{
						if (!cells[x][y][z]) continue;
						if (cellVis[x][y][z] == VIS_BORN) continue;

						int nb = CountNeighbors(x, y, z);
						if (nb < surviveMin)
							cellVis[x][y][z] = VIS_DYING_UNDER;
						else if (nb > surviveMax)
							cellVis[x][y][z] = VIS_DYING_OVER;
						else
							cellVis[x][y][z] = VIS_STABLE;
					}
		}

		// Classify all alive cells by predicted fate (used after seed/reseed)
		void ClassifyFates()
		{
			for (int x = 0; x < dim; x++)
				for (int y = 0; y < dim; y++)
					for (int z = 0; z < dim; z++)
					{
						if (!cells[x][y][z]) continue;

						int nb = CountNeighbors(x, y, z);
						if (nb < surviveMin)
							cellVis[x][y][z] = VIS_DYING_UNDER;
						else if (nb > surviveMax)
							cellVis[x][y][z] = VIS_DYING_OVER;
						else
							cellVis[x][y][z] = VIS_STABLE;
					}
		}

		int CountAlive() const
		{
			int n = 0;
			for (int x = 0; x < dim; x++)
				for (int y = 0; y < dim; y++)
					for (int z = 0; z < dim; z++)
						if (cells[x][y][z]) n++;
			return n;
		}
	};


	// ── Create colored VoxelObjects and set up materials ──────────

	inline void SetupColoredCubes(Tmpl8::Scene& scene, LifeState& life)
	{
		life.objBase = (int)scene.voxelObjects.size();

		// Side Order palette (desaturated pastels)
		//   Muted Teal    #8CB8B0  — stable
		//   Soft Yellow   #D8D0A0  — born
		//   Coral Pink    #E8A0A0  — dying underpopulation
		//   Pale Lavender #C8B8D8  — dying overpopulation
		const uint8_t pals[] = { PAL_STABLE, PAL_BORN, PAL_DYING_UNDER, PAL_DYING_OVER };
		for (int i = 0; i < 4; i++)
			scene.voxelObjects.push_back(VoxelObject(1, 1, 1, { pals[i] }));

		// Register materials for those palette indices
		// GetMat(v) → materials[MAT_COUNT + (v - 1)]
		auto makeMat = [](float3 color) -> Material
			{
				Material m;
				m.type = MaterialType::Emissive;
				m.albedo = color;
				m.emission = color;
				m.emissionStr = 1.5f;
				m.roughness = 0.4f;
				return m;
			};

		scene.materials[MAT_COUNT + (PAL_STABLE - 1)] = makeMat(float3(0.549f, 0.722f, 0.690f)); // Muted Teal #8CB8B0
		scene.materials[MAT_COUNT + (PAL_BORN - 1)] = makeMat(float3(0.847f, 0.816f, 0.627f)); // Soft Yellow #D8D0A0
		scene.materials[MAT_COUNT + (PAL_DYING_UNDER - 1)] = makeMat(float3(0.910f, 0.627f, 0.627f)); // Coral Pink  #E8A0A0
		scene.materials[MAT_COUNT + (PAL_DYING_OVER - 1)] = makeMat(float3(0.784f, 0.722f, 0.847f)); // Pale Lavender #C8B8D8
	}


	// ── Sync alive cells → voxel instances ────────────────────────

	inline void SyncInstances(
		const LifeState& life,
		Tmpl8::Scene& scene,
		float3 groupRotation = float3(0))
	{
		scene.voxelInstances.clear();

		const float gridExtent = 80.0f;
		const float cellSize = gridExtent / static_cast<float>(life.dim);
		const float cubeScale = cellSize / life.cubeSpacing;
		const float3 center(256.0f, 240.0f, 256.0f);
		const float3 originVec = center - float3(gridExtent * 0.5f);

		mat4 rot = mat4::RotateX(groupRotation.x)
			* mat4::RotateY(groupRotation.y)
			* mat4::RotateZ(groupRotation.z);

		for (int x = 0; x < life.dim; x++)
			for (int y = 0; y < life.dim; y++)
				for (int z = 0; z < life.dim; z++)
				{
					if (!life.cells[x][y][z]) continue;

					float3 localPos(
						originVec.x + (x + 0.5f) * cellSize,
						originVec.y + (y + 0.5f) * cellSize,
						originVec.z + (z + 0.5f) * cellSize
					);

					float3 offset = localPos - center;
					float4 rotated = rot * float4(offset, 1.0f);
					float3 pos = float3(rotated.x, rotated.y, rotated.z) + center;

					// Pick colored object based on cell fate
					int objIdx = life.objBase + (int)life.cellVis[x][y][z];

					VoxelFactory::CreateInstance(
						scene, objIdx,
						pos, groupRotation,
						float3(cubeScale)
					);
				}

		scene.RebuildDirtyInstances();
	}


	// ============================================================
	// Scene definition
	// ============================================================

	inline SceneDef LivingCubeShowcase()
	{
		SceneDef s;
		s.name = "Living Cube";

		// ── Objects ───────────────────────────────────────────────
		// Display platform — flattened into world grid
		s.voxObjects.push_back({
			"assets/Showcase/display_platform.vox",
			float3(256, 120, 256),
			float3(0, 0, 0),
			float3(1, 1, 1), true
			});

		// Colored cubes created programmatically in first tick

		// ── Camera ────────────────────────────────────────────────
		s.camPos = float3(0.5f, 0.47f, -0.28f);
		s.camTarget = float3(0.5f, 0.45f, 0.5f);

		// ── Sky ───────────────────────────────────────────────────
		s.sky.sunDir = normalize(float3(0.2f, -0.5f, 0.3f));
		s.sky.sunColor = float3(1.0f, 0.85f, 0.8f);
		s.sky.sunIntensity = 0.6f;
		s.sky.timeOfDay = 0.3f;
		s.sky.animate = false;

		// ── Lights ─────────────────────────────────────────────────
		auto addLight = [&](float3 pos, float3 col)
			{
				PointLight l;
				l.position = pos;
				l.color = col;
				l.enabled = true;
				s.pointLights.push_back(l);
			};
		addLight(float3(0.5f, 0.85f, 0.5f), float3(0.9f, 0.88f, 0.85f));   // overhead
		addLight(float3(0.5f, 0.45f, 0.1f), float3(0.5f, 0.48f, 0.45f));   // front
		addLight(float3(0.1f, 0.5f, 0.5f), float3(0.2f, 0.25f, 0.23f));   // left fill
		addLight(float3(0.9f, 0.5f, 0.5f), float3(0.22f, 0.2f, 0.25f));   // right fill
		addLight(float3(0.5f, 0.15f, 0.5f), float3(0.12f, 0.11f, 0.10f));  // below
		addLight(float3(0.5f, 0.5f, 0.9f), float3(0.15f, 0.18f, 0.22f));  // behind

		auto life = std::make_shared<LifeState>();
		life->ApplyPreset();
		life->SeedRandom(0.35f);

		// ── Tick ──────────────────────────────────────────────────
		s.tickCallback = [life](SceneDef& def, Tmpl8::Scene& scene,
			float deltaTime, std::function<void()> resetAcc)
			{
				if (life->needsInitialSync)
				{
					life->needsInitialSync = false;
					SetupColoredCubes(scene, *life);
					SyncInstances(*life, scene, life->GetRotation());
					if (resetAcc) resetAcc();
					return;
				}

				life->TickRotation(deltaTime);

				if (!life->paused)
				{
					life->timer += deltaTime * 0.001f;
					if (life->timer >= life->stepInterval)
					{
						life->timer -= life->stepInterval;
						life->Step();
					}
				}

				SyncInstances(*life, scene, life->GetRotation());
				if (resetAcc) resetAcc();
			};

		// ── UI ────────────────────────────────────────────────────
		s.uiCallback = [life](SceneDef& def, Tmpl8::Scene& scene,
			std::function<void()> resetAcc)
			{
				if (!ImGui::CollapsingHeader("Living Cube", ImGuiTreeNodeFlags_DefaultOpen))
					return;

				int alive = life->CountAlive();
				int total = life->dim * life->dim * life->dim;
				ImGui::Text("Gen %d  |  %d / %d alive  |  %d instances",
					life->generation, alive, total,
					(int)scene.voxelInstances.size());

				ImGui::Spacing();

				// ── Color legend ──────────────────────────────────
				auto colorDot = [](float3 c, const char* label)
					{
						ImGui::ColorButton(label,
							ImVec4(c.x, c.y, c.z, 1.0f),
							ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker,
							ImVec2(12, 12));
						ImGui::SameLine();
						ImGui::Text("%s", label);
					};
				colorDot(float3(0.549f, 0.722f, 0.690f), "Stable");
				ImGui::SameLine();
				colorDot(float3(0.847f, 0.816f, 0.627f), "Born");
				ImGui::SameLine();
				colorDot(float3(0.910f, 0.627f, 0.627f), "Underpop");
				ImGui::SameLine();
				colorDot(float3(0.784f, 0.722f, 0.847f), "Overpop");

				ImGui::Spacing();

				// ── Controls ──────────────────────────────────────
				if (ImGui::Button(life->paused ? "  Play  " : "  Pause "))
					life->paused = !life->paused;
				ImGui::SameLine();
				if (ImGui::Button("Step"))
				{
					life->Step();
					SyncInstances(*life, scene, life->GetRotation());
					if (resetAcc) resetAcc();
				}
				ImGui::SameLine();
				if (ImGui::Button("Reseed"))
				{
					life->SeedRandom(0.35f);
					SyncInstances(*life, scene, life->GetRotation());
					if (resetAcc) resetAcc();
				}
				ImGui::SameLine();
				if (ImGui::Button("Clear"))
				{
					life->Clear();
					SyncInstances(*life, scene, life->GetRotation());
					if (resetAcc) resetAcc();
				}

				ImGui::SliderFloat("Speed (s/step)", &life->stepInterval, 0.05f, 2.0f, "%.2f");

				ImGui::Spacing();
				ImGui::Separator();

				static const char* presetNames[] = {
					"Coral (4-6/5-6)", "Growth (5-8/6-7)",
					"Sprawl (4-8/5-7)", "Custom"
				};
				if (ImGui::Combo("Ruleset", &life->preset, presetNames, LifeState::P_COUNT))
				{
					life->ApplyPreset();
					life->SeedRandom(0.35f);
					SyncInstances(*life, scene, life->GetRotation());
					if (resetAcc) resetAcc();
				}

				if (life->preset == LifeState::P_CUSTOM)
				{
					ImGui::SliderInt("Survive Min", &life->surviveMin, 0, 26);
					ImGui::SliderInt("Survive Max", &life->surviveMax, 0, 26);
					ImGui::SliderInt("Birth Min", &life->birthMin, 0, 26);
					ImGui::SliderInt("Birth Max", &life->birthMax, 0, 26);
				}

				ImGui::Spacing();
				ImGui::Separator();

				if (ImGui::SliderInt("Grid Size", &life->dim, 4, LifeState::MAX_DIM))
				{
					life->dim = std::clamp(life->dim, 4, (int)LifeState::MAX_DIM);
					life->SeedRandom(0.35f);
					SyncInstances(*life, scene, life->GetRotation());
					if (resetAcc) resetAcc();
				}

				if (ImGui::SliderFloat("Cube Spacing", &life->cubeSpacing, 1.0f, 2.0f, "%.2f"))
				{
					SyncInstances(*life, scene, life->GetRotation());
					if (resetAcc) resetAcc();
				}

				ImGui::Spacing();

				static float density = 0.35f;
				ImGui::SliderFloat("Seed Density", &density, 0.05f, 0.8f, "%.2f");
				if (ImGui::Button("Reseed with Density"))
				{
					life->SeedRandom(density);
					SyncInstances(*life, scene, life->GetRotation());
					if (resetAcc) resetAcc();
				}

				ImGui::Spacing();

				if (ImGui::CollapsingHeader("Rotation"))
				{
					ImGui::SliderFloat("Speed X", &life->speedX, 0.0f, 1.0f, "%.2f rad/s");
					ImGui::SliderFloat("Speed Y", &life->speedY, 0.0f, 1.0f, "%.2f rad/s");
					ImGui::SliderFloat("Speed Z", &life->speedZ, 0.0f, 1.0f, "%.2f rad/s");
				}
			};

		return s;
	}

} // namespace GameScenes