#pragma once
#include "SceneManager.h"
#include <algorithm>
#include <cmath>

namespace GameScenes
{

	static constexpr int GLY_W = 5;
	static constexpr int GLY_H = 7;

	static constexpr uint8_t GLYPH_O[GLY_H] = {
		0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E
	};
	static constexpr uint8_t GLYPH_R[GLY_H] = {
		0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11
	};
	static constexpr uint8_t GLYPH_D[GLY_H] = {
		0x1C, 0x12, 0x11, 0x11, 0x11, 0x12, 0x1C
	};
	static constexpr uint8_t GLYPH_E[GLY_H] = {
		0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F
	};

	static const uint8_t* ORDER_GLYPHS[5] = {
		GLYPH_O, GLYPH_R, GLYPH_D, GLYPH_E, GLYPH_R
	};

	static constexpr float OUT_BPM = 79.0f;
	static constexpr float OUT_BEAT = 60.0f / OUT_BPM;
	static constexpr int OUT_SPHERES = 1002;
	static constexpr int OUT_LETTERS = 5;

	// --- Catmull-Rom ---

	inline float3 OutroCR(float3 p0, float3 p1, float3 p2, float3 p3, float t)
	{
		float t2 = t * t, t3 = t2 * t;
		return 0.5f * (
			(2.0f * p1) +
			(-p0 + p2) * t +
			(2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
			(-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3
			);
	}

	// evaluate a spline with nPts control points at t in [0,1]
	inline float3 OutroEval(const float3* pts, int nPts, float t)
	{
		int segs = nPts - 3;
		if (segs < 1) return pts[1];
		float s = std::clamp(t, 0.0f, 1.0f) * (float)segs;
		int i = std::clamp((int)s, 0, segs - 1);
		return OutroCR(pts[i], pts[i + 1], pts[i + 2], pts[i + 3], s - (float)i);
	}

	inline float outroSmooth(float t)
	{
		t = std::clamp(t, 0.0f, 1.0f);
		return t * t * (3.0f - 2.0f * t);
	}


	// --- Per sphere: 7-point spline defining its full journey ---

	struct OutroSphere
	{
		int    letter;
		float3 path[7];     // full Catmull-Rom path: screen -> bloom -> curve -> hover -> target
		float  delay;       // normalized start time [0..1] — controls stagger
		float  speed;       // how fast this sphere traverses (slight variation)
	};

	static constexpr uint MAT_OUT_WHITE = MAT_RANDOM_START + 10;
	static constexpr uint MAT_OUT_CORAL = MAT_RANDOM_START + 11;
	static constexpr uint MAT_OUT_TEAL = MAT_RANDOM_START + 13;
	static constexpr uint MAT_OUT_LAVENDER = MAT_RANDOM_START + 14;
	static constexpr uint MAT_OUT_BLUE = MAT_RANDOM_START + 15;

	inline void SetupOutroMaterials(Tmpl8::Scene& scene)
	{
		// white: Lambertian — reflects the neutral room lighting as true white
		{
			Material m;
			m.type = MaterialType::Lambertian;
			m.albedo = float3(0.98f, 0.98f, 1.0f);
			m.roughness = 0.3f;
			scene.materials[MAT_OUT_WHITE] = m;
		}

		// accents: emissive but low strength so they don't bleed
		auto emissive = [&](uint slot, float3 color, float str)
			{
				Material m;
				m.type = MaterialType::Emissive;
				m.albedo = color;
				m.emission = color;
				m.emissionStr = str;
				scene.materials[slot] = m;
			};

		emissive(MAT_OUT_CORAL, float3(0.91f, 0.63f, 0.63f), 1.5f);
		emissive(MAT_OUT_TEAL, float3(0.55f, 0.72f, 0.69f), 1.5f);
		emissive(MAT_OUT_LAVENDER, float3(0.78f, 0.72f, 0.85f), 1.5f);
		emissive(MAT_OUT_BLUE, float3(0.47f, 0.60f, 0.72f), 1.5f);
	}

	struct OutroState
	{
		// timing: just two things — travel duration and hold+fade
		float travelBeats = 16;   // how many beats for the full sphere journey
		float holdBeats = 4;    // stillness
		float fadeBeats = 4;    // fade to white

		float time = 0.0f;
		float cycleTime = 0.0f;
		bool  initialized = false;

		float sphereRadius = 0.005f;
		float shrinkMul = 1.0f;
		float fadeWhite = 0.0f;

		std::vector<OutroSphere> sph;
		std::vector<float3>      pos;
		std::vector<uint>        mats;

		float3 startCamPos = float3(0.5f, 0.48f, 0.15f);
		float3 startCamTarget = float3(0.5f, 0.45f, 0.5f);
		float3 camFwd, camRight, camUp;

		float3 textCenter;

		// camera spline
		static constexpr int CAM_PTS = 7;
		float3 camPath[CAM_PTS] = {
			float3(0.50f, 0.48f, 0.05f),
			float3(0.50f, 0.48f, 0.15f),
			float3(0.32f, 0.52f, 0.30f),
			float3(0.25f, 0.54f, 0.50f),
			float3(0.32f, 0.55f, 0.68f),
			float3(0.50f, 0.54f, 0.76f),
			float3(0.50f, 0.54f, 0.86f),
		};
		float3 camTgtStart = float3(0.50f, 0.45f, 0.50f);
		float3 camTgtEnd = float3(0.50f, 0.43f, 0.50f);


		float TravelSec() const { return travelBeats * OUT_BEAT; }
		float HoldStart() const { return TravelSec(); }
		float HoldSec()   const { return holdBeats * OUT_BEAT; }
		float FadeStart() const { return HoldStart() + HoldSec(); }
		float FadeSec()   const { return fadeBeats * OUT_BEAT; }
		float TotalSec()  const { return TravelSec() + HoldSec() + FadeSec(); }
		int TotalBeats()  const { return (int)(travelBeats + holdBeats + fadeBeats); }


		void UpdateCamera(float ct, SceneDef& def)
		{
			// camera stays still while screen is covered (first ~20%)
			// then arcs around during the flow
			// then settles for hold/fade
			float travelDur = TravelSec();
			float cp;
			if (ct < travelDur * 0.15f)
				cp = 0.0f;
			else if (ct < travelDur)
			{
				float p = (ct - travelDur * 0.15f) / (travelDur * 0.85f);
				cp = outroSmooth(p) * 0.95f;
			}
			else
			{
				float p = (ct - travelDur) / max(TotalSec() - travelDur, 0.001f);
				cp = 0.95f + outroSmooth(p) * 0.05f;
			}

			def.camPos = OutroEval(camPath, CAM_PTS, cp);
			def.camTarget = camTgtStart * (1.0f - cp) + camTgtEnd * cp;
		}


		void Build()
		{
			camFwd = normalize(startCamTarget - startCamPos);
			camRight = normalize(cross(camFwd, float3(0, 1, 0)));
			camUp = cross(camRight, camFwd);

			// text layout
			float cellSize = 0.012f;
			float letterSpacing = 2.0f;
			float totalWidth = (GLY_W * OUT_LETTERS +
				letterSpacing * (OUT_LETTERS - 1)) * cellSize;

			float textZ = 0.50f;
			float textStartX = 0.5f - totalWidth * 0.5f;
			float textCenterY = 0.43f;
			textCenter = float3(0.5f, textCenterY, textZ);

			// count cells
			int filledPerLetter[OUT_LETTERS] = {};
			int totalFilled = 0;
			for (int g = 0; g < OUT_LETTERS; g++)
				for (int r = 0; r < GLY_H; r++)
					for (int c = 0; c < GLY_W; c++)
						if (ORDER_GLYPHS[g][r] & (1 << (GLY_W - 1 - c)))
						{
							filledPerLetter[g]++;
							totalFilled++;
						}

			int perLetter[OUT_LETTERS] = {};
			int assigned = 0;
			for (int g = 0; g < OUT_LETTERS; g++)
			{
				perLetter[g] = (OUT_SPHERES * filledPerLetter[g]) / max(totalFilled, 1);
				assigned += perLetter[g];
			}
			perLetter[2] += OUT_SPHERES - assigned;

			sph.clear();
			sph.reserve(OUT_SPHERES);
			uint seed = 4242;

			const float goldenAngle = 2.39996323f;
			int globalIdx = 0;

			for (int g = 0; g < OUT_LETTERS; g++)
			{
				float letterX = textStartX + g * (GLY_W + letterSpacing) * cellSize;
				float3 letterCenter(
					letterX + GLY_W * cellSize * 0.5f,
					textCenterY,
					textZ
				);
				float3 hoverCenter(
					letterCenter.x,
					textCenterY + GLY_H * cellSize * 0.5f + 0.12f,
					textZ
				);

				std::vector<float3> cells;
				for (int r = 0; r < GLY_H; r++)
					for (int c = 0; c < GLY_W; c++)
						if (ORDER_GLYPHS[g][r] & (1 << (GLY_W - 1 - c)))
						{
							float cx = letterX + c * cellSize;
							float cy = textCenterY + (GLY_H * 0.5f - r) * cellSize;
							cells.push_back(float3(cx, cy, textZ));
						}

				int perCell = perLetter[g] / max((int)cells.size(), 1);
				int rem = perLetter[g] - perCell * (int)cells.size();
				int placed = 0;

				for (int ci = 0; ci < (int)cells.size() && placed < perLetter[g]; ci++)
				{
					int count = perCell + (ci < rem ? 1 : 0);
					for (int si = 0; si < count && placed < perLetter[g]; si++)
					{
						OutroSphere sp;
						sp.letter = g;

						// target: cell with jitter
						float3 target = cells[ci] + float3(
							(RandomFloat(seed) - 0.5f) * cellSize * 0.7f,
							(RandomFloat(seed) - 0.5f) * cellSize * 0.7f,
							(RandomFloat(seed) - 0.5f) * cellSize * 0.3f
						);

						// hover: above letter
						float3 hover = hoverCenter + float3(
							(RandomFloat(seed) - 0.5f) * 0.015f,
							(RandomFloat(seed) - 0.5f) * 0.015f,
							(RandomFloat(seed) - 0.5f) * 0.005f
						);

						// screen position: golden-angle disc on camera lens
						float spiralAngle = globalIdx * goldenAngle;
						float spiralR = sqrtf((float)globalIdx / (float)OUT_SPHERES) * 0.025f;
						float3 screenPos = startCamPos
							+ camFwd * 0.008f
							+ camRight * cosf(spiralAngle) * spiralR
							+ camUp * sinf(spiralAngle) * spiralR;

						// bloom: pulled back, spiral expanded
						float bloomAngle = spiralAngle + 1.2f;
						float bloomR = spiralR + 0.06f;
						float3 bloom = startCamPos
							+ camFwd * 0.18f
							+ camRight * cosf(bloomAngle) * bloomR
							+ camUp * sinf(bloomAngle) * bloomR;

						// curve waypoint: flowing S-curve between bloom and hover
						// offset sideways based on letter position for visual separation
						float letterBias = ((float)g / (float)(OUT_LETTERS - 1) - 0.5f) * 0.12f;
						float curveY = (bloom.y + hover.y) * 0.5f + 0.06f;
						float curveZ = (bloom.z + hover.z) * 0.5f;
						float3 curveWP = float3(
							0.5f + letterBias + sinf(spiralAngle) * 0.03f,
							curveY,
							curveZ
						);

						// build the 7-point spline
						// P0: tangent control behind screen (for smooth departure)
						sp.path[0] = screenPos - camFwd * 0.01f;
						// P1: screen position
						sp.path[1] = screenPos;
						// P2: bloom (spiral outward)
						sp.path[2] = bloom;
						// P3: flowing curve midpoint
						sp.path[3] = curveWP;
						// P4: hover above letter
						sp.path[4] = hover;
						// P5: target in letter
						sp.path[5] = target;
						// P6: tangent control past target (smooth arrival)
						sp.path[6] = target + float3(0, -0.005f, 0);

						// delay: based on normalized target Y (bottom fills first)
						// and normalized X (left fills first, slight offset)
						float xNorm = (target.x - (textCenter.x - totalWidth * 0.5f)) / totalWidth;
						xNorm = std::clamp(xNorm, 0.0f, 1.0f);

						// will be refined after Y-sort
						sp.delay = 0;
						sp.speed = 0.92f + RandomFloat(seed) * 0.16f; // 0.92 - 1.08

						sph.push_back(sp);
						globalIdx++;
						placed++;
					}
				}
			}

			// sort within each letter by Y ascending, assign delay
			int cumul = 0;
			for (int g = 0; g < OUT_LETTERS; g++)
			{
				int start = cumul;
				int end = cumul + perLetter[g];

				std::sort(sph.begin() + start, sph.begin() + end,
					[](const OutroSphere& a, const OutroSphere& b) {
						return a.path[5].y < b.path[5].y; // sort by target Y
					});

				for (int i = start; i < end; i++)
				{
					float yNorm = (float)(i - start) / (float)max(end - start - 1, 1);
					float xNorm = ((float)g / (float)(OUT_LETTERS - 1));

					// bottom-left arrives first, top-right arrives last
					// this creates the wave-fill effect
					sph[i].delay = yNorm * 0.35f + xNorm * 0.15f;
				}

				cumul = end;
			}

			pos.resize(OUT_SPHERES);
			for (int i = 0; i < OUT_SPHERES; i++)
				pos[i] = sph[i].path[1]; // start at screen pos

			// materials
			mats.clear();
			seed = 8888;
			const uint accentMats[4] = {
				MAT_OUT_CORAL, MAT_OUT_TEAL, MAT_OUT_LAVENDER, MAT_OUT_BLUE
			};
			for (int i = 0; i < OUT_SPHERES; i++)
			{
				float r = RandomFloat(seed);
				if (r < 0.80f)
					mats.push_back(MAT_OUT_WHITE);
				else
				{
					int idx = (int)((r - 0.80f) * 20.0f); // 0-3
					idx = std::clamp(idx, 0, 3);
					mats.push_back(accentMats[idx]);
				}
			}
		}


		void UpdatePositions(float ct)
		{
			float travelDur = TravelSec();
			float holdEnd = FadeStart();
			float fadeEnd = TotalSec();

			for (int i = 0; i < OUT_SPHERES; i++)
			{
				const OutroSphere& s = sph[i];

				if (ct < travelDur)
				{
					// global progress [0..1]
					float gp = ct / travelDur;

					// per-sphere progress: offset by delay, scaled by speed
					// the delay window is the first 50% of travel time
					float startT = s.delay * 0.50f;
					float localP = (gp - startT) / max(1.0f - startT, 0.001f);
					localP = std::clamp(localP, 0.0f, 1.0f);

					// apply per-sphere speed variation
					localP = std::clamp(localP * s.speed, 0.0f, 1.0f);

					// smooth the parameter for clean motion
					float t = outroSmooth(localP);

					// evaluate the sphere's personal spline
					pos[i] = OutroEval(s.path, 7, t);
				}
				else if (ct < holdEnd)
				{
					// hold: at target with gentle wave
					float holdT = ct - travelDur;
					float xNorm = (s.path[5].x - textCenter.x) / max(0.001f, 0.1f);
					float ripple = sinf(xNorm * 4.0f - holdT * 2.5f) * 0.001f;
					float beat = sinf(fmodf(ct, OUT_BEAT) / OUT_BEAT * 2.0f * PI) * 0.0005f;
					pos[i] = s.path[5] + float3(0, 0, ripple + beat);
				}
				else if (ct < fadeEnd)
				{
					// fade: drift upward gently
					float p = (ct - holdEnd) / max(FadeSec(), 0.001f);
					p = std::clamp(p, 0.0f, 1.0f);
					float drift = p * p * 0.03f;
					float3 outward = (s.path[5] - textCenter) * p * 0.2f;
					pos[i] = s.path[5] + float3(0, drift, 0) + outward;
				}
				else
				{
					pos[i] = s.path[5];
				}
			}
		}


		void Reset()
		{
			cycleTime = 0;
			time = 0;
			fadeWhite = 0;
			shrinkMul = 1.0f;
			for (int i = 0; i < OUT_SPHERES; i++)
				pos[i] = sph[i].path[1];
		}
	};


	inline SceneDef OutroShowcase()
	{
		SceneDef s;
		s.name = "Outro";
		s.useVoxelGrid = false;
		s.splineEnabled = false;

		s.camPos = float3(0.5f, 0.48f, 0.15f);
		s.camTarget = float3(0.5f, 0.45f, 0.5f);

		s.sky.sunDir = normalize(float3(0.2f, -0.8f, 0.1f));
		s.sky.sunColor = float3(1.0f, 0.98f, 0.95f);
		s.sky.sunIntensity = 2.5f;
		s.sky.timeOfDay = 0.30f;
		s.sky.animate = false;

		auto addL = [&](float3 p, float3 c)
			{
				PointLight l;
				l.position = p; l.color = c; l.enabled = true;
				s.pointLights.push_back(l);
			};
		addL(float3(0.5f, 0.80f, 0.50f), float3(0.8f, 0.8f, 0.85f));
		addL(float3(0.2f, 0.50f, 0.30f), float3(0.5f, 0.5f, 0.52f));
		addL(float3(0.8f, 0.50f, 0.30f), float3(0.5f, 0.5f, 0.52f));
		addL(float3(0.5f, 0.55f, 0.70f), float3(0.6f, 0.6f, 0.63f));
		addL(float3(0.5f, 0.35f, 0.50f), float3(0.4f, 0.4f, 0.43f));

		auto state = std::make_shared<OutroState>();

		s.tickCallback = [state](SceneDef& def, Tmpl8::Scene& scene,
			float deltaTime, std::function<void()> resetAcc)
			{
				if (!state->initialized)
				{
					state->initialized = true;
					SetupGyroscopeMaterials(scene);
					state->Build();
					if (resetAcc) resetAcc();
					return;
				}

				float dt = deltaTime * 0.001f;
				state->time += dt;
				state->cycleTime += dt;
				float ct = state->cycleTime;

				def.splineEnabled = false;
				state->UpdateCamera(ct, def);
				state->UpdatePositions(ct);

				// fade phase
				float fadeStart = state->FadeStart();
				float fadeDur = state->FadeSec();
				if (ct >= fadeStart)
				{
					float p = std::clamp((ct - fadeStart) / max(fadeDur, 0.001f), 0.0f, 1.0f);
					state->shrinkMul = max(0.0f, 1.0f - p * p * p);
					state->fadeWhite = outroSmooth(p);
				}
				else
				{
					state->shrinkMul = 1.0f;
					state->fadeWhite = 0.0f;
				}

				// rebuild spheres
				scene.spheres.clear();
				float r = state->sphereRadius * state->shrinkMul;
				if (r > 0.0001f)
				{
					for (int i = 0; i < OUT_SPHERES; i++)
					{
						scene.spheres.push_back({
							state->pos[i], r, state->mats[i]
							});
					}
				}
				scene.BuildSphereBVH();

				if (resetAcc) resetAcc();
			};

		s.uiCallback = [state](SceneDef& def, Tmpl8::Scene& scene,
			std::function<void()> resetAcc)
			{
				if (!ImGui::CollapsingHeader("Outro", ImGuiTreeNodeFlags_DefaultOpen))
					return;

				float beat = state->cycleTime / OUT_BEAT;
				float travelP = std::clamp(state->cycleTime / state->TravelSec(), 0.0f, 1.0f);
				bool inHold = state->cycleTime >= state->HoldStart() && state->cycleTime < state->FadeStart();
				bool inFade = state->cycleTime >= state->FadeStart();

				const char* phase = "TRAVEL";
				ImVec4 col(0.3f, 0.7f, 0.9f, 1);
				if (inHold) { phase = "HOLD"; col = ImVec4(0.5f, 0.5f, 0.8f, 1); }
				if (inFade) { phase = "FADE"; col = ImVec4(0.9f, 0.9f, 0.9f, 1); }

				ImGui::TextColored(col, "Phase: %s", phase);
				ImGui::SameLine();
				ImGui::TextDisabled("%.1f / %.1fs", state->cycleTime, state->TotalSec());

				ImGui::ProgressBar(state->cycleTime / state->TotalSec(), ImVec2(-1, 3));

				ImGui::Text("Beat %.1f / %d  |  Travel: %.0f%%",
					beat, state->TotalBeats(), travelP * 100.0f);
				ImGui::Text("Shrink: %.3f  |  Fade: %.3f",
					state->shrinkMul, state->fadeWhite);

				ImGui::Spacing();

				bool changed = false;
				changed |= ImGui::SliderFloat("Travel##out", &state->travelBeats, 8, 24, "%.0f beats");
				changed |= ImGui::SliderFloat("Hold##out", &state->holdBeats, 1, 8, "%.0f beats");
				changed |= ImGui::SliderFloat("Fade##out", &state->fadeBeats, 2, 8, "%.0f beats");

				if (ImGui::TreeNode("Camera##outro"))
				{
					float cp = state->cycleTime / state->TravelSec();
					ImGui::TextDisabled("Cam progress: %.3f", std::clamp(cp, 0.0f, 1.0f));
					ImGui::TextDisabled("Pos: %.2f %.2f %.2f",
						def.camPos.x, def.camPos.y, def.camPos.z);
					ImGui::TreePop();
				}

				if (ImGui::Button("Reset##outro"))
				{
					state->Reset();
					if (resetAcc) resetAcc();
				}
			};

		return s;
	}

}