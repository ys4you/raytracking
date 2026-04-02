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

	static constexpr float OUTRO_BPM = 79.0f;
	static constexpr float OUTRO_BEAT = 60.0f / OUTRO_BPM;
	static constexpr int OUTRO_SPHERES = 1002;
	static constexpr int OUTRO_LETTERS = 5;

	// ---- Mini 5x7 font for credits ----

	struct CreditGlyph { uint8_t rows[7]; };

	inline const CreditGlyph* GetCreditGlyph(char ch)
	{
		static const CreditGlyph glyphs[] = {
			{{0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}}, // 0  C
			{{0x00, 0x00, 0x16, 0x19, 0x10, 0x10, 0x10}}, // 1  r
			{{0x00, 0x00, 0x0E, 0x11, 0x1F, 0x10, 0x0E}}, // 2  e
			{{0x00, 0x00, 0x0E, 0x01, 0x0F, 0x11, 0x0F}}, // 3  a
			{{0x08, 0x08, 0x1C, 0x08, 0x08, 0x09, 0x06}}, // 4  t
			{{0x01, 0x01, 0x0F, 0x11, 0x11, 0x11, 0x0F}}, // 5  d
			{{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}, // 6  space
			{{0x10, 0x10, 0x1E, 0x11, 0x11, 0x11, 0x1E}}, // 7  b
			{{0x00, 0x00, 0x11, 0x11, 0x0F, 0x01, 0x0E}}, // 8  y
			{{0x00, 0x04, 0x00, 0x00, 0x00, 0x04, 0x00}}, // 9  :
			{{0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04}}, // 10 Y
			{{0x00, 0x00, 0x0F, 0x10, 0x0E, 0x01, 0x1E}}, // 11 s
			{{0x0E, 0x11, 0x10, 0x0E, 0x01, 0x11, 0x0E}}, // 12 S
			{{0x04, 0x00, 0x0C, 0x04, 0x04, 0x04, 0x0E}}, // 13 i
			{{0x02, 0x00, 0x06, 0x02, 0x02, 0x12, 0x0C}}, // 14 j
			{{0x00, 0x00, 0x16, 0x19, 0x11, 0x11, 0x11}}, // 15 n
			{{0x00, 0x00, 0x1A, 0x15, 0x15, 0x11, 0x11}}, // 16 m
			{{0x00, 0x00, 0x11, 0x11, 0x11, 0x13, 0x0D}}, // 17 u
			{{0x00, 0x00, 0x0E, 0x10, 0x10, 0x11, 0x0E}}, // 18 c
			{{0x00, 0x00, 0x0E, 0x11, 0x11, 0x11, 0x0E}}, // 19 o
			{{0x00, 0x00, 0x11, 0x11, 0x11, 0x0A, 0x04}}, // 20 v
		};

		switch (ch)
		{
		case 'C': return &glyphs[0];
		case 'r': return &glyphs[1];
		case 'e': return &glyphs[2];
		case 'a': return &glyphs[3];
		case 't': return &glyphs[4];
		case 'd': return &glyphs[5];
		case ' ': return &glyphs[6];
		case 'b': return &glyphs[7];
		case 'y': return &glyphs[8];
		case ':': return &glyphs[9];
		case 'Y': return &glyphs[10];
		case 's': return &glyphs[11];
		case 'S': return &glyphs[12];
		case 'i': return &glyphs[13];
		case 'j': return &glyphs[14];
		case 'n': return &glyphs[15];
		case 'm': return &glyphs[16];
		case 'u': return &glyphs[17];
		case 'c': return &glyphs[18];
		case 'o': return &glyphs[19];
		case 'v': return &glyphs[20];
		default:  return &glyphs[6];
		}
	}

	struct CreditSphere
	{
		float3 target;
		float  revealTime;
		float3 glitchOffset;
		uint   seed;
		bool   highlight = false;

	};

	// ---- Catmull-Rom helpers ----

	inline float3 OutroCR(const float3 p0, const float3 p1, const float3 p2, const float3 p3, const float t)
	{
		float t2 = t * t, t3 = t2 * t;
		return 0.5f * (
			(2.0f * p1) +
			(-p0 + p2) * t +
			(2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
			(-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3
			);
	}

	inline float3 OutroEval(const float3* pts, const int nPts, const float t)
	{
		int segs = nPts - 3;
		if (segs < 1) return pts[1];
		// Convert normalized path time into segment-space for Catmull-Rom evaluation.
		float s = std::clamp(t, 0.0f, 1.0f) * (float)segs;
		int i = std::clamp((int)s, 0, segs - 1);
		return OutroCR(pts[i], pts[i + 1], pts[i + 2], pts[i + 3], s - (float)i);
	}

	inline float outroSmooth(float t)
	{
		t = std::clamp(t, 0.0f, 1.0f);
		return t * t * (3.0f - 2.0f * t);
	}

	struct OutroSphere
	{
		int    letter;
		float3 path[7];
		float  delay;
		float  speed;
	};

	// ---- Materials ----

	static constexpr uint MAT_OUT_WHITE = MAT_RANDOM_START + 10;
	static constexpr uint MAT_OUT_CORAL = MAT_RANDOM_START + 11;
	static constexpr uint MAT_OUT_TEAL = MAT_RANDOM_START + 13;
	static constexpr uint MAT_OUT_LAVENDER = MAT_RANDOM_START + 14;
	static constexpr uint MAT_OUT_BLUE = MAT_RANDOM_START + 15;
	static constexpr uint MAT_OUT_RED_SPHERE = MAT_RANDOM_START + 16;
	static constexpr uint MAT_OUT_CREDIT = MAT_RANDOM_START + 17;
	static constexpr uint MAT_OUT_CREDIT_NAME = MAT_RANDOM_START + 18;

	inline void SetupOutroMaterials(Tmpl8::Scene& scene)
	{
		{
			Material m;
			m.type = MaterialType::Lambertian;
			m.albedo = float3(0.98f, 0.98f, 1.0f);
			m.roughness = 0.3f;
			scene.materials[MAT_OUT_WHITE] = m;
		}

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
		emissive(MAT_OUT_CREDIT_NAME, float3(0.98f, 0.98f, 1.0f), 4.0f);


		{
			Material m;
			m.type = MaterialType::Emissive;
			m.albedo = float3(0.78f, 0.31f, 0.31f);
			m.emission = float3(0.78f, 0.31f, 0.31f);
			m.emissionStr = 3.0f;
			scene.materials[MAT_OUT_RED_SPHERE] = m;
		}

		// credit text: pale lavender glow
		emissive(MAT_OUT_CREDIT, float3(0.78f, 0.72f, 0.85f), 2.0f);
	}

	// ---- State ----

	struct OutroState
	{
		float travelBeats = 16;
		float holdBeats = 10;
		float fadeBeats = 8;

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

		float redSphereRadius = 0.0f;
		float redSphereMaxRadius = 0.10f;
		float3 redSpherePos;
		bool redSphereActive = false;

		// credits
		std::vector<CreditSphere> creditSpheres;
		std::vector<float3>       creditPos;
		float creditRadius = 0.002f;
		float creditGlitchDur = 0.5f;

		float TravelSec() const { return travelBeats * OUTRO_BEAT; }
		float HoldStart() const { return TravelSec(); }
		float HoldSec()   const { return holdBeats * OUTRO_BEAT; }
		float FadeStart() const { return HoldStart() + HoldSec(); }
		float FadeSec()   const { return fadeBeats * OUTRO_BEAT; }
		float TotalSec()  const { return TravelSec() + HoldSec() + FadeSec(); }
		int TotalBeats()  const { return (int)(travelBeats + holdBeats + fadeBeats); }

		void UpdateCamera(float ct, SceneDef& def)
		{
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

		void BuildCreditLine(const char* text, float3 origin, float cellSz,
			float baseRevealTime, uint& seed, bool highlight = false)
		{
			float cursorX = 0.0f;
			for (int ci = 0; text[ci]; ci++)
			{
				char ch = text[ci];
				if (ch == '\n')
				{
					cursorX = 0.0f;
					origin.y -= cellSz * 9.0f;
					continue;
				}

				const CreditGlyph* g = GetCreditGlyph(ch);
				if (!g) { cursorX += cellSz * 6.0f; continue; }

				float charReveal = baseRevealTime + ci * 0.04f;

				for (int row = 0; row < 7; row++)
				{
					for (int col = 0; col < 5; col++)
					{
						if (!(g->rows[row] & (1 << (4 - col)))) continue;

						CreditSphere cs;
						cs.target = float3(
							origin.x + cursorX + col * cellSz,
							origin.y + (3.5f - row) * cellSz,
							origin.z
						);
						cs.revealTime = charReveal + RandomFloat(seed) * 0.08f;
						cs.glitchOffset = float3(
							(RandomFloat(seed) - 0.5f) * 0.04f,
							(RandomFloat(seed) - 0.5f) * 0.04f,
							(RandomFloat(seed) - 0.5f) * 0.02f
						);
						cs.seed = seed;
						cs.highlight = highlight;
						seed = seed * 1103515245 + 12345;

						creditSpheres.push_back(cs);
					}
				}

				cursorX += cellSz * 6.0f;
			}
		}

		void Build()
		{
			camFwd = normalize(startCamTarget - startCamPos);
			camRight = normalize(cross(camFwd, float3(0, 1, 0)));
			camUp = cross(camRight, camFwd);

			float cellSize = 0.012f;
			float letterSpacing = 2.0f;
			float totalWidth = (GLY_W * OUTRO_LETTERS +
				letterSpacing * (OUTRO_LETTERS - 1)) * cellSize;

			float textZ = 0.50f;
			float textStartX = 0.5f - totalWidth * 0.5f;
			float textCenterY = 0.43f;
			textCenter = float3(0.5f, textCenterY, textZ);

			float convergenceX = 0.0f;
			for (int g = 1; g <= 3; g++)
			{
				float lx = textStartX + g * (GLY_W + letterSpacing) * cellSize;
				convergenceX += lx + GLY_W * cellSize * 0.5f;
			}
			convergenceX /= 3.0f;
			redSpherePos = float3(convergenceX, textCenterY, textZ + 0.12f);

			// --- ORDER letter spheres ---
			int filledPerLetter[OUTRO_LETTERS] = {};
			int totalFilled = 0;
			for (int g = 0; g < OUTRO_LETTERS; g++)
				for (int r = 0; r < GLY_H; r++)
					for (int c = 0; c < GLY_W; c++)
						if (ORDER_GLYPHS[g][r] & (1 << (GLY_W - 1 - c)))
						{
							filledPerLetter[g]++;
							totalFilled++;
						}

			int perLetter[OUTRO_LETTERS] = {};
			int assigned = 0;
			for (int g = 0; g < OUTRO_LETTERS; g++)
			{
				perLetter[g] = (OUTRO_SPHERES * filledPerLetter[g]) / max(totalFilled, 1);
				assigned += perLetter[g];
			}
			perLetter[2] += OUTRO_SPHERES - assigned;

			sph.clear();
			sph.reserve(OUTRO_SPHERES);
			uint seed = 4242;
			const float goldenAngle = 2.39996323f;
			int globalIdx = 0;

			for (int g = 0; g < OUTRO_LETTERS; g++)
			{
				float letterX = textStartX + g * (GLY_W + letterSpacing) * cellSize;
				float3 letterCenter(letterX + GLY_W * cellSize * 0.5f, textCenterY, textZ);
				float3 hoverCenter(letterCenter.x, textCenterY + GLY_H * cellSize * 0.5f + 0.12f, textZ);

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

						float3 target = cells[ci] + float3(
							(RandomFloat(seed) - 0.5f) * cellSize * 0.7f,
							(RandomFloat(seed) - 0.5f) * cellSize * 0.7f,
							(RandomFloat(seed) - 0.5f) * cellSize * 0.3f);

						float3 hover = hoverCenter + float3(
							(RandomFloat(seed) - 0.5f) * 0.015f,
							(RandomFloat(seed) - 0.5f) * 0.015f,
							(RandomFloat(seed) - 0.5f) * 0.005f);

						float spiralAngle = globalIdx * goldenAngle;
						float spiralR = sqrtf((float)globalIdx / (float)OUTRO_SPHERES) * 0.12f;
						float3 screenPos = startCamPos + camFwd * 0.03f
							+ camRight * cosf(spiralAngle) * spiralR
							+ camUp * sinf(spiralAngle) * spiralR;

						float bloomAngle = spiralAngle + 1.2f;
						float bloomR = spiralR + 0.06f;
						float3 bloom = startCamPos + camFwd * 0.18f
							+ camRight * cosf(bloomAngle) * bloomR
							+ camUp * sinf(bloomAngle) * bloomR;

						float letterBias = ((float)g / (float)(OUTRO_LETTERS - 1) - 0.5f) * 0.12f;
						float3 curveWP = float3(
							0.5f + letterBias + sinf(spiralAngle) * 0.03f,
							(bloom.y + hover.y) * 0.5f + 0.06f,
							(bloom.z + hover.z) * 0.5f);

						sp.path[0] = screenPos - camFwd * 0.01f;
						sp.path[1] = screenPos;
						sp.path[2] = bloom;
						sp.path[3] = curveWP;
						sp.path[4] = hover;
						sp.path[5] = target;
						sp.path[6] = target + float3(0, -0.005f, 0);
						sp.delay = 0;
						sp.speed = 0.92f + RandomFloat(seed) * 0.16f;

						sph.push_back(sp);
						globalIdx++;
						placed++;
					}
				}
			}

			int cumul = 0;
			for (int g = 0; g < OUTRO_LETTERS; g++)
			{
				int start = cumul;
				int end = cumul + perLetter[g];
				std::sort(sph.begin() + start, sph.begin() + end,
					[](const OutroSphere& a, const OutroSphere& b) {
						return a.path[5].y < b.path[5].y;
					});
				for (int i = start; i < end; i++)
				{
					float yNorm = (float)(i - start) / (float)max(end - start - 1, 1);
					float xNorm = ((float)g / (float)(OUTRO_LETTERS - 1));
					sph[i].delay = yNorm * 0.35f + xNorm * 0.15f;
				}
				cumul = end;
			}

			pos.resize(OUTRO_SPHERES);
			for (int i = 0; i < OUTRO_SPHERES; i++)
				pos[i] = sph[i].path[1];

			mats.clear();
			seed = 8888;
			const uint accentMats[4] = { MAT_OUT_CORAL, MAT_OUT_TEAL, MAT_OUT_LAVENDER, MAT_OUT_BLUE };
			for (int i = 0; i < OUTRO_SPHERES; i++)
			{
				float r = RandomFloat(seed);
				if (r < 0.80f) mats.push_back(MAT_OUT_WHITE);
				else
				{
					int idx = std::clamp((int)((r - 0.80f) * 20.0f), 0, 3);
					mats.push_back(accentMats[idx]);
				}
			}

			// --- Credit text spheres ---
			creditSpheres.clear();
			float creditCellSize = 0.004f;

			float3 creditBL(
				textStartX - 0.03f,                                    // more to the left
				textCenterY - GLY_H * cellSize * 0.5f - 0.06f,        // was -0.025f
				textZ - 0.005f
			);


			BuildCreditLine("Created by:", creditBL, creditCellSize, 0.0f, seed, false);

			float3 nameBL(
				creditBL.x,
				creditBL.y - creditCellSize * 9.0f,
				creditBL.z
			);
			BuildCreditLine("    Yesse Seijn", nameBL, creditCellSize * 1.4f, 0.5f, seed, true);

			// top-right credits
			float3 creditTR(
				textStartX + totalWidth - 0.06f,
				textCenterY + GLY_H * cellSize * 0.5f + 0.025f,
				textZ - 0.005f
			);
			BuildCreditLine("music by:", creditTR, creditCellSize, 1.0f, seed, false);

			float3 musicTR(
				creditTR.x,
				creditTR.y - creditCellSize * 9.0f,
				creditTR.z
			);
			BuildCreditLine("    musinova", musicTR, creditCellSize, 1.5f, seed, true);

			creditPos.resize(creditSpheres.size());
			for (int i = 0; i < (int)creditSpheres.size(); i++)
				creditPos[i] = float3(-10, -10, -10);
		}

		void UpdatePositions(float ct)
		{
			float travelDur = TravelSec();
			float holdEnd = FadeStart();
			float fadeEnd = TotalSec();

			for (int i = 0; i < OUTRO_SPHERES; i++)
			{
				const OutroSphere& s = sph[i];
				if (ct < travelDur)
				{
					float gp = ct / travelDur;
					float startT = s.delay * 0.50f;
					float localP = std::clamp((gp - startT) / max(1.0f - startT, 0.001f), 0.0f, 1.0f);
					localP = std::clamp(localP * s.speed, 0.0f, 1.0f);
					pos[i] = OutroEval(s.path, 7, outroSmooth(localP));
				}
				else if (ct < holdEnd)
				{
					float holdT = ct - travelDur;
					float xNorm = (s.path[5].x - textCenter.x) / max(0.001f, 0.1f);
					float ripple = sinf(xNorm * 4.0f - holdT * 2.5f) * 0.001f;
					float beat = sinf(fmodf(ct, OUTRO_BEAT) / OUTRO_BEAT * 2.0f * PI) * 0.0005f;
					pos[i] = s.path[5] + float3(0, 0, ripple + beat);
				}
				else if (ct < fadeEnd)
				{
					float p = std::clamp((ct - holdEnd) / max(FadeSec(), 0.001f), 0.0f, 1.0f);
					pos[i] = s.path[5] + float3(0, p * p * 0.03f, 0)
						+ (s.path[5] - textCenter) * p * 0.2f;
				}
				else
				{
					pos[i] = s.path[5];
				}
			}
		}

		void UpdateCredits(float ct)
		{
			float holdTime = ct - HoldStart();

			for (int i = 0; i < (int)creditSpheres.size(); i++)
			{
				const CreditSphere& cs = creditSpheres[i];
				float age = holdTime - cs.revealTime;

				if (age < 0.0f)
				{
					creditPos[i] = float3(-10, -10, -10);
				}
				else if (age < creditGlitchDur)
				{
					float p = age / creditGlitchDur;
					float settle = outroSmooth(p);

					uint s = cs.seed + (uint)(age * 30.0f);
					float jitterAmt = (1.0f - settle) * 0.8f;
					float spike = (RandomFloat(s) < (1.0f - p) * 0.3f) ? 1.5f : 0.0f;

					float3 jitter(
						(RandomFloat(s) - 0.5f) * (jitterAmt + spike) * 0.02f,
						(RandomFloat(s) - 0.5f) * (jitterAmt + spike) * 0.02f,
						(RandomFloat(s) - 0.5f) * jitterAmt * 0.005f
					);

					creditPos[i] = cs.target + cs.glitchOffset * (1.0f - settle) + jitter;
				}
				else
				{
					uint s = cs.seed + (uint)(ct * 20.0f);
					float micro = 0.0002f;
					creditPos[i] = cs.target + float3(
						(RandomFloat(s) - 0.5f) * micro,
						(RandomFloat(s) - 0.5f) * micro, 0);
				}
			}
		}

		void Reset()
		{
			cycleTime = 0; time = 0;
			fadeWhite = 0; shrinkMul = 1.0f;
			redSphereRadius = 0.0f; redSphereActive = false;
			for (int i = 0; i < OUTRO_SPHERES; i++) pos[i] = sph[i].path[1];
			for (int i = 0; i < (int)creditSpheres.size(); i++)
				creditPos[i] = float3(-10, -10, -10);
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

		addL(float3(0.5f, 0.80f, 0.50f), float3(1.5f, 1.5f, 1.55f));
		addL(float3(0.2f, 0.50f, 0.30f), float3(1.0f, 1.0f, 1.05f));
		addL(float3(0.8f, 0.50f, 0.30f), float3(1.0f, 1.0f, 1.05f));
		addL(float3(0.5f, 0.55f, 0.70f), float3(1.2f, 1.2f, 1.25f));
		addL(float3(0.5f, 0.35f, 0.50f), float3(0.8f, 0.8f, 0.85f));

		auto state = std::make_shared<OutroState>();

		s.tickCallback = [state](SceneDef& def, Tmpl8::Scene& scene,
			float deltaTime, std::function<void()> resetAcc)
			{
				if (!state->initialized)
				{
					state->initialized = true;
					SetupOutroMaterials(scene);
					state->Build();

					scene.spheres.clear();
					for (int i = 0; i < OUTRO_SPHERES; i++)
						scene.spheres.push_back({
							state->pos[i], state->sphereRadius, state->mats[i] });
					scene.BuildSphereBVH();
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
                state->shrinkMul = 1.0f; // DON'T shrink — keep everything visible for white-out
                state->fadeWhite = outroSmooth(p);

                // red sphere transitions: red → white, emission ramps up massively
                float3 redColor = float3(0.78f, 0.31f, 0.31f);
                float3 whiteColor = float3(1.0f, 1.0f, 1.0f);
                float3 fadeColor = redColor * (1.0f - p) + whiteColor * p;
                float fadeEmission = 3.0f + p * p * 60.0f; // 3 → 63

                Material m;
                m.type = MaterialType::Emissive;
                m.albedo = fadeColor;
                m.emission = fadeColor;
                m.emissionStr = fadeEmission;
                scene.materials[MAT_OUT_RED_SPHERE] = m;

                // grow the sphere bigger during fade
                state->redSphereMaxRadius = 0.10f + p * 0.08f; // 0.10 → 0.18
            }
            else
            {
                state->shrinkMul = 1.0f;
                state->fadeWhite = 0.0f;
            }

            // red sphere growth
            float redStart = state->TravelSec() * 0.75f;
            float redEnd = state->HoldStart() + state->HoldSec() * 0.5f;
            if (ct >= redStart)
            {
                float growP = std::clamp((ct - redStart) / max(redEnd - redStart, 0.001f), 0.0f, 1.0f);
                float eased = 1.0f - (1.0f - growP) * (1.0f - growP) * (1.0f - growP);
                state->redSphereRadius = eased * state->redSphereMaxRadius;
                state->redSphereActive = true;
            }
            else
            {
                state->redSphereRadius = 0.0f;
                state->redSphereActive = false;
            }

				// credits
				if (ct >= state->HoldStart())
					state->UpdateCredits(ct);

				// rebuild all spheres
				scene.spheres.clear();
				// grow from 0 during first 15% of travel
				float growIn = std::clamp(ct / (state->TravelSec() * 0.15f), 0.0f, 1.0f);
				float r = state->sphereRadius * growIn * state->shrinkMul;

				if (r > 0.0001f)
					for (int i = 0; i < OUTRO_SPHERES; i++)
						scene.spheres.push_back({ state->pos[i], r, state->mats[i] });

				if (state->redSphereActive && state->redSphereRadius > 0.001f)
					scene.spheres.push_back({
						state->redSpherePos, state->redSphereRadius, MAT_OUT_RED_SPHERE });

			if (ct >= state->HoldStart())
				{
					float creditR = state->creditRadius * state->shrinkMul;
					float nameR = state->creditRadius * 1.4f * state->shrinkMul;
					if (creditR > 0.0001f)
						for (int i = 0; i < (int)state->creditSpheres.size(); i++)
							if (state->creditPos[i].x > -1.0f)
								scene.spheres.push_back({
									state->creditPos[i],
									state->creditSpheres[i].highlight ? nameR : creditR,
									state->creditSpheres[i].highlight ? MAT_OUT_CREDIT_NAME : MAT_OUT_CREDIT
									});
				}

				scene.BuildSphereBVH();
				if (resetAcc) resetAcc();
			};

		s.uiCallback = [state](SceneDef& def, Tmpl8::Scene& scene,
			std::function<void()> resetAcc)
			{
				if (!ImGui::CollapsingHeader("Outro", ImGuiTreeNodeFlags_DefaultOpen))
					return;

				float beat = state->cycleTime / OUTRO_BEAT;
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
				ImGui::Text("Shrink: %.3f  |  Fade: %.3f  |  Red: %.4f",
					state->shrinkMul, state->fadeWhite, state->redSphereRadius);
				ImGui::Text("Credits: %d spheres  |  Total: %d",
					(int)state->creditSpheres.size(), (int)scene.spheres.size());

				ImGui::Spacing();
				ImGui::SliderFloat("Travel##out", &state->travelBeats, 8, 24, "%.0f beats");
				ImGui::SliderFloat("Hold##out", &state->holdBeats, 1, 8, "%.0f beats");
				ImGui::SliderFloat("Fade##out", &state->fadeBeats, 2, 8, "%.0f beats");
				ImGui::SliderFloat("Red Max R##out", &state->redSphereMaxRadius, 0.02f, 0.15f, "%.3f");
				ImGui::SliderFloat("Credit R##out", &state->creditRadius, 0.001f, 0.005f, "%.3f");
				ImGui::SliderFloat("Glitch Dur##out", &state->creditGlitchDur, 0.1f, 2.0f, "%.2fs");

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