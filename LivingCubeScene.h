#pragma once
#include "SceneManager.h"
#include "VoxelFactory.h"
#include "VoxelObject.h"
#include <vector>
#include <cstring>
#include <algorithm>

namespace GameScenes
{

	enum CellVis : uint8_t
	{
		VIS_STABLE = 0,
		VIS_BORN = 1,
		VIS_DYING_UNDER = 2,
		VIS_DYING_OVER = 3
	};

	static constexpr uint8_t PAL_STABLE = 250;
	static constexpr uint8_t PAL_BORN = 251;
	static constexpr uint8_t PAL_DYING_UNDER = 252;
	static constexpr uint8_t PAL_DYING_OVER = 253;


	struct LifeState
	{
		static constexpr int   MAX_DIM = 8;
		static constexpr float GRID_SIZE = 512.0f;

		int     dim = 8;
		bool    cells[MAX_DIM][MAX_DIM][MAX_DIM] = {};
		bool    buffer[MAX_DIM][MAX_DIM][MAX_DIM] = {};
		CellVis cellVis[MAX_DIM][MAX_DIM][MAX_DIM] = {};
		int     generation = 0;

		int surviveMin = 4, surviveMax = 6;
		int birthMin = 5, birthMax = 6;

		float stepInterval = 0.2f;
		float timer = 0.0f;
		bool  paused = false;
		bool  needsInitialSync = true;

		float cubeSpacing = 1.05f;

		float angleX = 0.0f, angleY = 0.0f, angleZ = 0.0f;
		float speedX = 0.18f, speedY = 0.35f, speedZ = 0.12f;

		int objBase = 0;

		void TickRotation(const float deltaTimeMs)
		{
			const float dt = deltaTimeMs * 0.001f;
			angleX += speedX * dt;
			angleY += speedY * dt;
			angleZ += speedZ * dt;
			if (angleX > 2.0f * PI) angleX -= 2.0f * PI;
			if (angleY > 2.0f * PI) angleY -= 2.0f * PI;
			if (angleZ > 2.0f * PI) angleZ -= 2.0f * PI;
		}

		float3 GetRotation() const { return float3(angleX, angleY, angleZ); }

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

		void SeedRandom(float density = 0.45f)
		{
			Clear();
			int pad = dim / 4;
			for (int x = pad; x < dim - pad; x++)
				for (int y = pad; y < dim - pad; y++)
					for (int z = pad; z < dim - pad; z++)
						cells[x][y][z] = (RandomFloat() < density);
			ClassifyFates();
		}

		int CountNeighbors(const int cx, const int cy, const int cz) const
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

			for (int x = 0; x < dim; x++)
				for (int y = 0; y < dim; y++)
					for (int z = 0; z < dim; z++)
					{
						if (!buffer[x][y][z]) continue;
						cellVis[x][y][z] = (!cells[x][y][z])
							? VIS_BORN
							: VIS_STABLE;
					}

			memcpy(cells, buffer, sizeof(cells));
			generation++;

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



	inline void SetupColoredCubes(Tmpl8::Scene& scene, LifeState& life)
	{
		life.objBase = (int)scene.voxelObjects.size();

		const uint8_t pals[] = { PAL_STABLE, PAL_BORN, PAL_DYING_UNDER, PAL_DYING_OVER };
		for (int i = 0; i < 4; i++)
			scene.voxelObjects.push_back(VoxelObject(1, 1, 1, { pals[i] }));

		auto makeMat = [](float3 color, float str) -> Material
			{
				Material m;
				m.type = MaterialType::Emissive;
				m.albedo = color;
				m.emission = color;
				m.emissionStr = str;
				m.roughness = 0.3f;
				return m;
			};

		scene.materials[MAT_COUNT + (PAL_STABLE - 1)] = makeMat(float3(0.55f, 0.72f, 0.69f), 4.0f);
		scene.materials[MAT_COUNT + (PAL_BORN - 1)] = makeMat(float3(1.0f, 0.95f, 0.6f), 5.0f);
		scene.materials[MAT_COUNT + (PAL_DYING_UNDER - 1)] = makeMat(float3(0.95f, 0.4f, 0.4f), 4.5f);
		scene.materials[MAT_COUNT + (PAL_DYING_OVER - 1)] = makeMat(float3(0.78f, 0.55f, 0.95f), 4.5f);
	}



	inline void SyncInstances(
		const LifeState& life,
		Tmpl8::Scene& scene,
		float3 groupRotation = float3(0))
	{
		scene.voxelInstances.clear();

		const float gridExtent = 160.0f;
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

					int objIdx = life.objBase + (int)life.cellVis[x][y][z];

					VoxelFactory::CreateInstance(
						scene, objIdx,
						pos, groupRotation,
						float3(cubeScale)
					);
				}

		scene.RebuildDirtyInstances();
	}



	inline SceneDef LivingCubeShowcase()
	{
		SceneDef s;
		s.name = "Living Cube";

		s.voxObjects.push_back({
			"assets/Showcase/display_platform.vox",
			float3(256, 120, 256),
			float3(0, 0, 0),
			float3(1, 1, 1), true
			});
		s.camPos = float3(0.5f, 0.55f, -0.05f);
		s.camTarget = float3(0.5f, 0.47f, 0.5f);

		s.sky.sunDir = normalize(float3(0.2f, -0.5f, 0.3f));
		s.sky.sunColor = float3(1.0f, 0.95f, 0.9f);
		s.sky.sunIntensity = 1.5f;
		s.sky.timeOfDay = 0.35f;
		s.sky.animate = false;

		// orbiting spline camera
		float3 center(0.5f, 0.47f, 0.5f);
		float orbitR = 0.45f;
		float camY = 0.55f;
		int pts = 8;
		for (int i = 0; i < pts; i++)
		{
			float a = (float)i / (float)pts * 2.0f * PI;
			s.splinePoints.push_back(float3(
				center.x + cosf(a) * orbitR,
				camY + sinf(a * 2.0f) * 0.04f,  // gentle vertical bob
				center.z + sinf(a) * orbitR
			));
		}
		s.splineSpeed = 0.06f;
		s.splineEnabled = true;

		// strong 8-light rig
		auto addL = [&](float3 p, float3 c)
			{
				PointLight l;
				l.position = p; l.color = c; l.enabled = true;
				s.pointLights.push_back(l);
			};
		addL(float3(0.5f, 0.90f, 0.5f), float3(1.8f, 1.75f, 1.7f));    // overhead key
		addL(float3(0.5f, 0.10f, 0.5f), float3(0.4f, 0.38f, 0.36f));   // floor bounce
		addL(float3(0.0f, 0.50f, 0.5f), float3(0.5f, 0.65f, 0.6f));    // teal left
		addL(float3(1.0f, 0.50f, 0.5f), float3(0.6f, 0.5f, 0.7f));     // lavender right
		addL(float3(0.5f, 0.50f, 0.0f), float3(1.2f, 1.15f, 1.1f));    // front fill
		addL(float3(0.5f, 0.50f, 1.0f), float3(0.5f, 0.6f, 0.7f));     // blue back rim
		addL(float3(0.2f, 0.75f, 0.2f), float3(0.3f, 0.4f, 0.35f));    // top-left accent
		addL(float3(0.8f, 0.75f, 0.8f), float3(0.35f, 0.3f, 0.4f));    // top-right accent

		auto life = std::make_shared<LifeState>();
		life->ApplyPreset();
		life->SeedRandom(0.45f);

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
				colorDot(float3(1.0f, 0.95f, 0.6f), "Born");
				ImGui::SameLine();
				colorDot(float3(0.95f, 0.4f, 0.4f), "Underpop");
				ImGui::SameLine();
				colorDot(float3(0.78f, 0.55f, 0.95f), "Overpop");

				ImGui::Spacing();

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
					life->SeedRandom(0.45f);
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
					life->SeedRandom(0.45f);
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
					life->SeedRandom(0.45f);
					SyncInstances(*life, scene, life->GetRotation());
					if (resetAcc) resetAcc();
				}

				if (ImGui::SliderFloat("Cube Spacing", &life->cubeSpacing, 1.0f, 2.0f, "%.2f"))
				{
					SyncInstances(*life, scene, life->GetRotation());
					if (resetAcc) resetAcc();
				}

				ImGui::Spacing();

				static float density = 0.45f;
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

}