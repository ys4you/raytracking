
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

	static constexpr uint8_t PAL_BM_WORLD = 230;
	static constexpr uint8_t PAL_BM_EMPTY = 231;
	static constexpr uint8_t PAL_BM_FULL = 232;
	static constexpr uint8_t PAL_BM_FLASH = 234;

	static constexpr float BM_BPM = 79.0f;
	static constexpr float BM_BEAT = 60.0f / BM_BPM;
	static constexpr float BM_RAD2DEG = 180.0f / 3.14159265f;

	static constexpr int BM_L1 = 4;
	static constexpr int BM_L2 = 8;


	struct BrickmapState
	{
		enum Phase : int
		{
			SOLID = 0,
			SPLIT_L1,
			CULL_L1,
			SPLIT_L2,
			CULL_L2,
			HOLD,
			COLLAPSE,
			PHASE_COUNT
		};

		Phase phase = SOLID;
		Phase prevPhase = SOLID;
		float phaseTimer = 0.0f;
		float camSpeedRequest = 0.15f;
		float camHoldTimer = 0.0f;


		float gapL1 = 0.0f;
		float markL1 = 0.0f;
		float cullL1 = 0.0f;
		float splitL2 = 0.0f;
		float cullL2 = 0.0f;

		float bloomSpike = 0.0f;
		float bloomDecay = 8.0f;

		float durBeats[PHASE_COUNT] = { 3, 2, 3, 2, 3, 4, 2 };
		float snapBeats = 0.4f;

		bool  paused = false;
		bool  looping = true;
		float speedMul = 1.0f;

		float angleX = 0, angleY = 0, angleZ = 0;
		float speedX = 0.04f, speedY = 0.08f, speedZ = 0.03f;

		bool fineGrid[BM_L2][BM_L2][BM_L2] = {};
		bool coarseOcc[BM_L1][BM_L1][BM_L1] = {};

		int  objBase = -1;
		bool needsInit = true;

		static float easeOut(float t)
		{
			t = std::clamp(t, 0.0f, 1.0f);
			return 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
		}

		static float manhattan(int x, int y, int z, float c)
		{
			return fabsf(x - c) + fabsf(y - c) + fabsf(z - c);
		}

		static float staggeredCull(float globalCull, int x, int y, int z, float c, float maxDist)
		{
			if (globalCull <= 0.0f) return 0.0f;
			if (globalCull >= 1.0f) return 1.0f;
			float dist = manhattan(x, y, z, c) / maxDist;
			float localT = (globalCull - (1.0f - dist) * 0.4f) / 0.6f;
			return std::clamp(localT, 0.0f, 1.0f);
		}

		void Seed()
		{
			memset(fineGrid, 0, sizeof(fineGrid));
			memset(coarseOcc, 0, sizeof(coarseOcc));

			int count = 12 + (rand() % 8);
			for (int i = 0; i < count; i++)
				fineGrid[rand() % BM_L2][rand() % BM_L2][rand() % BM_L2] = true;

			for (int cx = 0; cx < BM_L1; cx++)
				for (int cy = 0; cy < BM_L1; cy++)
					for (int cz = 0; cz < BM_L1; cz++)
					{
						bool any = false;
						for (int dx = 0; dx < 2 && !any; dx++)
							for (int dy = 0; dy < 2 && !any; dy++)
								for (int dz = 0; dz < 2 && !any; dz++)
									if (fineGrid[cx * 2 + dx][cy * 2 + dy][cz * 2 + dz])
										any = true;
						coarseOcc[cx][cy][cz] = any;
					}
		}

		int CountCoarseOccupied() const
		{
			int n = 0;
			for (int x = 0; x < BM_L1; x++)
				for (int y = 0; y < BM_L1; y++)
					for (int z = 0; z < BM_L1; z++)
						if (coarseOcc[x][y][z]) n++;
			return n;
		}

		int CountFineOccupied() const
		{
			int n = 0;
			for (int x = 0; x < BM_L2; x++)
				for (int y = 0; y < BM_L2; y++)
					for (int z = 0; z < BM_L2; z++)
						if (fineGrid[x][y][z]) n++;
			return n;
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

		void TickPhase(float dtMs)
		{
			if (paused) return;
			prevPhase = phase;
			phaseTimer += dtMs * 0.001f * speedMul;

			float dur = durBeats[phase] * BM_BEAT;
			if (phaseTimer >= dur)
			{
				phaseTimer = 0.0f;
				int next = (int)phase + 1;
				if (next >= PHASE_COUNT)
				{
					next = looping ? SOLID : HOLD;
					if (looping) Seed();
				}

				phase = (Phase)next;
				if (!looping && phase == HOLD) paused = true;

				if (phase == CULL_L1 || phase == CULL_L2)
					bloomSpike = 0.6f;
				else if (phase == SPLIT_L1 || phase == SPLIT_L2)
					bloomSpike = 0.3f;
				else if (phase == COLLAPSE)
					bloomSpike = 0.4f;
			}

			float snap = easeOut(phaseTimer / (snapBeats * BM_BEAT));

			switch (phase)
			{
			case SOLID:
				gapL1 = 0; markL1 = 0; cullL1 = 0; splitL2 = 0; cullL2 = 0;
				break;
			case SPLIT_L1:
				gapL1 = snap; markL1 = 0; cullL1 = 0; splitL2 = 0; cullL2 = 0;
				break;
			case CULL_L1:
			{
				gapL1 = 1;
				float halfDur = durBeats[phase] * BM_BEAT * 0.5f;
				if (phaseTimer < halfDur)
				{
					markL1 = easeOut(phaseTimer / (snapBeats * BM_BEAT));
					cullL1 = 0;
				}
				else
				{
					markL1 = 1;
					cullL1 = easeOut((phaseTimer - halfDur) / (snapBeats * BM_BEAT));
				}
				splitL2 = 0; cullL2 = 0;
				break;
			}
			case SPLIT_L2:
				gapL1 = 1; markL1 = 1; cullL1 = 1;
				splitL2 = snap; cullL2 = 0;
				break;
			case CULL_L2:
			{
				gapL1 = 1; markL1 = 1; cullL1 = 1; splitL2 = 1;
				float halfDur = durBeats[phase] * BM_BEAT * 0.5f;
				if (phaseTimer < halfDur)
					cullL2 = 0;
				else
					cullL2 = easeOut((phaseTimer - halfDur) / (snapBeats * BM_BEAT));
				break;
			}
			case HOLD:
				gapL1 = 1; markL1 = 1; cullL1 = 1; splitL2 = 1; cullL2 = 1;
				break;
			case COLLAPSE:
			{
				float rev = 1.0f - snap;
				gapL1 = rev; markL1 = rev; cullL1 = rev * rev;
				splitL2 = rev; cullL2 = rev;
				break;
			}
			}
		}

		float3 GetRot() const { return float3(angleX, angleY, angleZ); }

		void Reset()
		{
			phase = SOLID; prevPhase = SOLID;
			phaseTimer = 0;
			gapL1 = markL1 = cullL1 = splitL2 = cullL2 = 0;
			bloomSpike = 0;
		}
	};


	inline std::shared_ptr<BrickmapState>& GetBrickmapState()
	{
		static std::shared_ptr<BrickmapState> inst;
		return inst;
	}


	inline void SetupBrickmapCubes(Tmpl8::Scene& scene, BrickmapState& bm)
	{
		bm.objBase = (int)scene.voxelObjects.size();

		scene.voxelObjects.push_back(VoxelObject(1, 1, 1, { PAL_BM_WORLD }));
		scene.voxelObjects.push_back(VoxelObject(1, 1, 1, { PAL_BM_EMPTY }));
		scene.voxelObjects.push_back(VoxelObject(1, 1, 1, { PAL_BM_FULL }));
		scene.voxelObjects.push_back(VoxelObject(1, 1, 1, { PAL_BM_FLASH }));

		auto mat = [](float3 c, float r) -> Material
			{
				Material m;
				m.type = MaterialType::Lambertian;
				m.albedo = c;
				m.roughness = r;
				return m;
			};

		scene.materials[MAT_COUNT + (PAL_BM_WORLD - 1)] = mat(float3(1, 1, 1), 0.25f);
		scene.materials[MAT_COUNT + (PAL_BM_EMPTY - 1)] = mat(float3(0.96f, 0.96f, 0.96f), 0.30f);
		scene.materials[MAT_COUNT + (PAL_BM_FULL - 1)] = mat(float3(0.549f, 0.722f, 0.690f), 0.25f);

		{
			Material m;
			m.type = MaterialType::Emissive;
			m.albedo = float3(1, 1, 1);
			m.emission = float3(1, 0.98f, 0.95f);
			m.emissionStr = 6.0f;
			m.roughness = 0.1f;
			scene.materials[MAT_COUNT + (PAL_BM_FLASH - 1)] = m;
		}
	}


	inline void SyncBrickmap(
		const BrickmapState& bm,
		Tmpl8::Scene& scene,
		float3 rotRad)
	{
		scene.voxelInstances.clear();

		const float gridExt = 80.0f;
		const float3 center(256, 240, 256);

		float3 rotDeg = rotRad * BM_RAD2DEG;

		mat4 rot = mat4::RotateX(rotRad.x)
			* mat4::RotateY(rotRad.y)
			* mat4::RotateZ(rotRad.z);

		auto orbit = [&](float3 p) -> float3
			{
				float3 o = p - center;
				float4 r = rot * float4(o, 1);
				return float3(r.x, r.y, r.z) + center;
			};

		if (bm.gapL1 < 0.001f)
		{
			VoxelFactory::CreateInstance(
				scene, bm.objBase + 0,
				orbit(center), rotDeg,
				float3(gridExt)
			);
			scene.RebuildDirtyInstances();
			return;
		}

		const float cL1 = (BM_L1 - 1) * 0.5f;
		const float l1Size = gridExt / (float)BM_L1;
		const float l1Gap = 5.0f;
		const float maxDistL1 = cL1 * 3.0f;

		bool showL2 = bm.splitL2 > 0.01f;

		for (int cx = 0; cx < BM_L1; cx++)
			for (int cy = 0; cy < BM_L1; cy++)
				for (int cz = 0; cz < BM_L1; cz++)
				{
					bool coarseOcc = bm.coarseOcc[cx][cy][cz];

					float3 l1Pos(
						center.x - gridExt * 0.5f + (cx + 0.5f) * l1Size,
						center.y - gridExt * 0.5f + (cy + 0.5f) * l1Size,
						center.z - gridExt * 0.5f + (cz + 0.5f) * l1Size
					);
					float3 dir((float)cx - cL1, (float)cy - cL1, (float)cz - cL1);
					l1Pos = l1Pos + dir * l1Gap * bm.gapL1;

					if (!coarseOcc)
					{
						float localCull = BrickmapState::staggeredCull(
							bm.cullL1, cx, cy, cz, cL1, maxDistL1);

						if (localCull >= 1.0f) continue;

						float s = l1Size * (1.01f - 0.13f * bm.gapL1) * (1.0f - localCull);
						if (s < 0.5f) continue;

						int objIdx = (localCull > 0.3f && localCull < 0.85f)
							? bm.objBase + 3
							: bm.objBase + 1;

						VoxelFactory::CreateInstance(
							scene, objIdx,
							orbit(l1Pos), rotDeg,
							float3(s)
						);
						continue;
					}

					if (!showL2)
					{
						float s = l1Size * (1.01f - 0.13f * bm.gapL1);
						int objIdx = (bm.markL1 > 0.5f)
							? bm.objBase + 2
							: bm.objBase + 1;

						VoxelFactory::CreateInstance(
							scene, objIdx,
							orbit(l1Pos), rotDeg,
							float3(s)
						);
						continue;
					}

					const float l2Size = l1Size / 2.0f;
					const float l2Gap = 2.0f * bm.splitL2;
					const float cL2 = 0.5f;
					const float maxDistL2 = cL2 * 3.0f;

					for (int sx = 0; sx < 2; sx++)
						for (int sy = 0; sy < 2; sy++)
							for (int sz = 0; sz < 2; sz++)
							{
								int fx = cx * 2 + sx;
								int fy = cy * 2 + sy;
								int fz = cz * 2 + sz;
								bool fineOcc = bm.fineGrid[fx][fy][fz];

								float3 l2Pos(
									l1Pos.x + (sx - 0.5f) * l2Size,
									l1Pos.y + (sy - 0.5f) * l2Size,
									l1Pos.z + (sz - 0.5f) * l2Size
								);

								float3 subDir(sx - 0.5f, sy - 0.5f, sz - 0.5f);
								l2Pos = l2Pos + subDir * l2Gap;

								float l2Scale = l2Size * (1.01f - 0.13f * bm.splitL2);

								if (!fineOcc)
								{
									float localCull = BrickmapState::staggeredCull(
										bm.cullL2, sx, sy, sz, cL2, maxDistL2);

									if (localCull >= 1.0f) continue;
									float s = l2Scale * (1.0f - localCull);
									if (s < 0.3f) continue;

									int objIdx = (localCull > 0.3f && localCull < 0.85f)
										? bm.objBase + 3
										: bm.objBase + 1;

									VoxelFactory::CreateInstance(
										scene, objIdx,
										orbit(l2Pos), rotDeg,
										float3(s)
									);
								}
								else
								{
									VoxelFactory::CreateInstance(
										scene, bm.objBase + 2,
										orbit(l2Pos), rotDeg,
										float3(l2Scale)
									);
								}
							}
				}

		scene.RebuildDirtyInstances();
	}



	inline SceneDef BrickmapShowcase()
	{
		SceneDef s;
		s.name = "Brickmap Viz";
		s.useVoxelGrid = true;

		s.voxObjects.push_back({
			"assets/Showcase/display_platform.vox",
			float3(256, 120, 256), float3(0), float3(1), true
			});

		s.camPos = float3(0.28f, 0.60f, 0.20f);
		s.camTarget = float3(0.50f, 0.45f, 0.50f);

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
		addL(float3(0.50f, 0.45f, 0.80f), float3(0.2f, 0.22f, 0.25f));

		auto& bm = GetBrickmapState();
		bm = std::make_shared<BrickmapState>();
		bm->Seed();

		struct BloomState { float baseIntensity = 0.3f; };
		auto bloom = std::make_shared<BloomState>();

		s.tickCallback = [bm, bloom](SceneDef&, Tmpl8::Scene& scene,
			float dt, std::function<void()> reset)
			{
				scene.instancesShadows = false;
				if (bm->needsInit)
				{
					bm->needsInit = false;
					SetupBrickmapCubes(scene, *bm);
					SyncBrickmap(*bm, scene, bm->GetRot());
					if (reset) reset();
					return;
				}

				BrickmapState::Phase oldPhase = bm->phase;
				float oldGap = bm->gapL1;
				float oldCull = bm->cullL1;
				float oldSplit = bm->splitL2;
				float oldCull2 = bm->cullL2;

				bm->TickRotation(dt);
				bm->TickPhase(dt);

				if (bm->phase != oldPhase &&
					bm->phase != BrickmapState::COLLAPSE &&
					bm->phase != BrickmapState::SOLID)
				{
					bm->camHoldTimer = BM_BEAT;
				}

				if (bm->camHoldTimer > 0.0f)
					bm->camHoldTimer -= dt * 0.001f;

				float dur = bm->durBeats[bm->phase] * BM_BEAT;
				float progress = (dur > 0) ? bm->phaseTimer / dur : 0;

				float camSpeed;
				if (bm->camHoldTimer > 0.0f)
				{
					camSpeed = 0.0f;
				}
				else if (bm->phase == BrickmapState::CULL_L1 || bm->phase == BrickmapState::CULL_L2)
				{
					camSpeed = 0.0f;
				}
				else if (progress < 0.15f)
				{
					float ramp = progress / 0.15f;
					camSpeed = 0.15f * ramp * ramp;
				}
				else if (progress > 0.7f)
				{
					float brake = 1.0f - (progress - 0.7f) / 0.3f;
					camSpeed = 0.15f * brake * brake;
				}
				else
				{
					camSpeed = 0.15f;
				}

				bm->camSpeedRequest = camSpeed;

				SyncBrickmap(*bm, scene, bm->GetRot());

				bool structureChanged =
					bm->phase != oldPhase ||
					fabsf(bm->gapL1 - oldGap) > 0.001f ||
					fabsf(bm->cullL1 - oldCull) > 0.001f ||
					fabsf(bm->splitL2 - oldSplit) > 0.001f ||
					fabsf(bm->cullL2 - oldCull2) > 0.001f;

				if (structureChanged && reset) reset();
			};

		s.uiCallback = [bm](SceneDef&, Tmpl8::Scene& scene,
			std::function<void()> reset)
			{
				if (!ImGui::CollapsingHeader("Brickmap Visualization", ImGuiTreeNodeFlags_DefaultOpen))
					return;

				static const char* names[] = {
					"SOLID", "SPLIT L1", "CULL L1",
					"SPLIT L2", "CULL L2", "HOLD", "COLLAPSE"
				};

				int coarseOcc = bm->CountCoarseOccupied();
				int fineOcc = bm->CountFineOccupied();

				ImGui::Text("Phase: %s", names[bm->phase]);
				ImGui::Text("L1: %d/%d coarse  |  L2: %d/%d voxels",
					coarseOcc, BM_L1 * BM_L1 * BM_L1,
					fineOcc, BM_L2 * BM_L2 * BM_L2);
				ImGui::Text("%d instances  |  bloom: %.2f",
					(int)scene.voxelInstances.size(), bm->bloomSpike);

				float dur = bm->durBeats[bm->phase] * BM_BEAT;
				ImGui::ProgressBar(dur > 0 ? bm->phaseTimer / dur : 0, ImVec2(-1, 3));

				ImGui::Spacing();

				auto dot = [](float3 c, const char* l)
					{
						ImGui::ColorButton(l, ImVec4(c.x, c.y, c.z, 1),
							ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker,
							ImVec2(12, 12));
						ImGui::SameLine(); ImGui::Text("%s", l);
					};
				dot(float3(1), "World");
				ImGui::SameLine();
				dot(float3(0.96f), "Empty");
				ImGui::SameLine();
				dot(float3(0.549f, 0.722f, 0.690f), "Occupied");
				ImGui::SameLine();
				dot(float3(1, 0.98f, 0.95f), "Flash");

				ImGui::Spacing(); ImGui::Separator();

				if (ImGui::Button(bm->paused ? "  Play  " : " Pause  "))
					bm->paused = !bm->paused;
				ImGui::SameLine();
				if (ImGui::Button("Reset"))
				{
					bm->Reset();
					SyncBrickmap(*bm, scene, bm->GetRot());
					if (reset) reset();
				}
				ImGui::SameLine();
				ImGui::Checkbox("Loop", &bm->looping);

				ImGui::SliderFloat("Speed##bm", &bm->speedMul, 0.25f, 4.0f, "%.2fx");
				ImGui::SliderFloat("Snap##bm", &bm->snapBeats, 0.1f, 2.0f, "%.1f beats");

				if (ImGui::CollapsingHeader("Timing (beats)"))
				{
					ImGui::SliderFloat("Solid##bm", &bm->durBeats[0], 1, 8, "%.0f");
					ImGui::SliderFloat("Split L1##bm", &bm->durBeats[1], 1, 8, "%.0f");
					ImGui::SliderFloat("Cull L1##bm", &bm->durBeats[2], 1, 8, "%.0f");
					ImGui::SliderFloat("Split L2##bm", &bm->durBeats[3], 1, 8, "%.0f");
					ImGui::SliderFloat("Cull L2##bm", &bm->durBeats[4], 1, 8, "%.0f");
					ImGui::SliderFloat("Hold##bm", &bm->durBeats[5], 1, 8, "%.0f");
					ImGui::SliderFloat("Collapse##bm", &bm->durBeats[6], 1, 8, "%.0f");
				}

				if (ImGui::CollapsingHeader("Effects"))
				{
					ImGui::SliderFloat("Bloom Decay##bm", &bm->bloomDecay, 2, 20, "%.1f/s");
				}

				if (ImGui::CollapsingHeader("Rotation"))
				{
					ImGui::SliderFloat("X##bm", &bm->speedX, 0, 1, "%.2f");
					ImGui::SliderFloat("Y##bm", &bm->speedY, 0, 1, "%.2f");
					ImGui::SliderFloat("Z##bm", &bm->speedZ, 0, 1, "%.2f");
				}
			};

		return s;
	}

}