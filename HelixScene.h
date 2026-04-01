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

	static constexpr uint8_t PAL_HX_REST = 241;
	static constexpr uint8_t PAL_HX_PULSE = 242;
	static constexpr uint8_t PAL_HX_FLASH = 243;

	static constexpr float HX_BPM = 79.0f;
	static constexpr float HX_BEAT = 60.0f / HX_BPM;
	static constexpr float HX_RAD2DEG = 180.0f / 3.14159265f;
	static constexpr int   HX_MAX_RUNGS = 28;
	static constexpr float HX_DURATION = 16.0f * HX_BEAT;  // ~12.15s


	inline float hsmooth(float t)
	{
		t = std::clamp(t, 0.0f, 1.0f);
		return t * t * (3.0f - 2.0f * t);
	}

	// ease-out: fast start, gentle settle
	inline float easeOut(float t)
	{
		t = std::clamp(t, 0.0f, 1.0f);
		float inv = 1.0f - t;
		return 1.0f - inv * inv * inv;
	}

	// ease-out quintic: even faster start
	inline float easeOutQ(float t)
	{
		t = std::clamp(t, 0.0f, 1.0f);
		float inv = 1.0f - t;
		return 1.0f - inv * inv * inv * inv * inv;
	}

	// remap p into a sub-window, return 0..1
	inline float window(float p, float start, float end)
	{
		return std::clamp((p - start) / (end - start), 0.0f, 1.0f);
	}

	// lerp
	inline float mix(float a, float b, float t) { return a + (b - a) * t; }


	struct HelixState
	{
		// ============ TIME ============
		float time = 0.0f;
		float progress = 0.0f;  // 0..1 over 12.15s

		// ============ EVALUATED SHAPE ============
		float radius = 0.0f;
		float height = 0.0f;
		float twist = 0.0f;
		float cubeSize = 0.0f;
		float rungReveal = 0.0f;
		int   strandCount = 1;
		float strand2Fade = 0.0f;

		// ============ ROTATION ============
		float helixSpin = 0.0f;
		float angleX = 0, angleY = 0, angleZ = 0;

		// ============ DUAL SCAN LINES ============
		float scanUp = -0.2f;   // ascending scan
		float scanDn = 1.2f;    // descending scan (appears later)
		float scanUpSpd = 0.0f;
		float scanDnSpd = 0.0f;
		float scanWidth = 0.0f;
		bool  scanDnActive = false;

		// ============ MICRO-MOTION ============
		float breathPhase = 0.0f;
		float breathAmp = 0.0f;
		float driftPhase = 0.0f;
		float driftAmp = 0.0f;
		float tiltAmp = 0.0f;

		// ============ ENGINE ============
		bool  paused = false;
		int   objBase = -1;
		bool  needsInit = true;


		// ===================================================================
		//  STAGGERED TIMELINE — each property on its own ease-out curve
		//  with different start times so nothing moves in lockstep.
		//  All curves are continuous. No events.
		// ===================================================================

		void Eval()
		{
			float p = progress;

			// ---- RUNGS: fastest to fill, 0%–60% of duration ----
			// 10 -> 28, aggressive ease-out so most appear in first 4s
			rungReveal = mix(10.0f, 28.0f, easeOutQ(window(p, 0.0f, 0.60f)));

			// ---- RADIUS: 0%–55% ----
			radius = mix(12.0f, 24.0f, easeOut(window(p, 0.0f, 0.55f)));

			// ---- HEIGHT: slightly delayed, 2%–65% ----
			height = mix(40.0f, 95.0f, easeOut(window(p, 0.02f, 0.65f)));

			// ---- TWIST: 5%–70%, slower ramp gives the coiling visual time ----
			twist = mix(1.0f, 3.5f, easeOut(window(p, 0.05f, 0.70f)));

			// ---- CUBE SIZE: quick settle, 0%–35% ----
			cubeSize = mix(4.0f, 5.2f, easeOut(window(p, 0.0f, 0.35f)));

			// ---- SECOND STRAND: 25%–40%, ~3s–5s mark ----
			float s2 = easeOut(window(p, 0.25f, 0.40f));
			strand2Fade = s2;
			strandCount = (p > 0.23f) ? 2 : 1;

			// ---- SCAN UP: always active, accelerates ----
			scanUpSpd = mix(0.25f, 0.45f, easeOut(window(p, 0.0f, 0.50f)));
			scanWidth = mix(0.10f, 0.06f, easeOut(window(p, 0.0f, 0.80f)));

			// ---- SCAN DOWN: activates at 45%, catches up in speed ----
			scanDnActive = (p > 0.43f);
			scanDnSpd = mix(0.0f, 0.40f, easeOut(window(p, 0.43f, 0.70f)));

			// ---- SPIN: accelerates smoothly ----
			float spinRate = mix(0.15f, 0.28f, easeOut(window(p, 0.0f, 0.60f)));
			helixSpin += spinRate * lastDt;

			// ---- GROUP ROTATION: imperceptible -> barely perceptible ----
			float grp = mix(0.008f, 0.022f, easeOut(window(p, 0.0f, 0.70f)));
			angleX += grp * 0.5f * lastDt;
			angleY += grp * lastDt;
			angleZ += grp * 0.25f * lastDt;

			// ---- MICRO-MOTION: ramps up, peaks at ~70% ----
			breathAmp = mix(0.2f, 1.0f, easeOut(window(p, 0.0f, 0.70f)));
			driftAmp = mix(0.2f, 0.6f, easeOut(window(p, 0.0f, 0.60f)));
			tiltAmp = mix(2.0f, 7.0f, easeOut(window(p, 0.0f, 0.65f)));
		}

		float lastDt = 0.0f;

		void Tick(float dtMs)
		{
			if (paused) return;
			float dt = dtMs * 0.001f;
			lastDt = dt;

			time += dt;
			progress = std::clamp(time / HX_DURATION, 0.0f, 1.0f);

			Eval();

			// advance scans
			scanUp += scanUpSpd * dt;
			if (scanUp > 1.3f) scanUp = -0.3f;

			if (scanDnActive)
			{
				scanDn -= scanDnSpd * dt;
				if (scanDn < -0.3f) scanDn = 1.3f;
			}

			// oscillators
			breathPhase += 0.5f * dt;
			driftPhase += 0.35f * dt;
		}


		// ========== SAMPLING ==========

		float GetScanAmp(float t) const
		{
			float w = std::max(scanWidth, 0.02f);

			// ascending scan
			float dU = fabsf(t - scanUp);
			float aU = (dU < w * 4.0f) ? expf(-(dU * dU) / (w * w)) : 0.0f;

			// descending scan
			float aD = 0.0f;
			if (scanDnActive)
			{
				float dD = fabsf(t - scanDn);
				aD = (dD < w * 4.0f) ? expf(-(dD * dD) / (w * w)) : 0.0f;
			}

			return std::clamp(aU + aD, 0.0f, 1.0f);
		}

		float GetRadius(float t) const
		{
			return radius + sinf(breathPhase + t * PI * 2.0f) * breathAmp;
		}

		float3 GetMicroDrift(int strand, float t) const
		{
			float ph = driftPhase + t * 3.0f;
			return float3(
				sinf(ph * 0.7f + strand * 2.1f) * driftAmp,
				sinf(ph * 0.5f + 0.8f) * driftAmp * 0.5f,
				cosf(ph * 0.6f + strand * 1.4f) * driftAmp
			);
		}

		float3 GetCubeRot(int strand, float t) const
		{
			float ph = driftPhase * 0.5f + t * 2.0f + strand * PI;

			// scan-proximate cubes tilt more — they're being "inspected"
			float scan = GetScanAmp(t);
			float amp = tiltAmp + scan * 8.0f;

			return float3(
				sinf(ph * 0.9f) * amp,
				sinf(ph * 0.6f + 0.3f) * amp,
				0.0f
			);
		}

		float GetRungAlpha(int i) const
		{
			float diff = rungReveal - (float)i;
			if (diff <= 0.0f) return 0.0f;
			if (diff >= 1.0f) return 1.0f;
			return hsmooth(diff);
		}

		float3 GetRot() const { return float3(angleX, angleY, angleZ); }

		const char* GetPhaseLabel() const
		{
			if (progress < 0.01f) return "DORMANT";
			if (progress < 0.30f) return "EMERGENCE";
			if (progress < 0.55f) return "ASSEMBLY";
			if (progress < 0.80f) return "COMPLETION";
			if (progress < 1.0f)  return "SETTLING";
			return "OPERATIONAL";
		}

		void Reset()
		{
			time = 0; progress = 0; lastDt = 0;
			radius = 12; height = 40; twist = 1; cubeSize = 4;
			rungReveal = 10; strandCount = 1; strand2Fade = 0;
			helixSpin = 0; angleX = 0; angleY = 0; angleZ = 0;
			scanUp = -0.2f; scanDn = 1.2f; scanUpSpd = 0; scanDnSpd = 0;
			scanWidth = 0.10f; scanDnActive = false;
			breathPhase = 0; breathAmp = 0.2f; driftPhase = 0;
			driftAmp = 0.2f; tiltAmp = 2.0f;
			paused = false;
		}
	};


	inline std::shared_ptr<HelixState>& GetHelixState()
	{
		static std::shared_ptr<HelixState> inst;
		return inst;
	}


	inline void SetupHelixCubes(Tmpl8::Scene& scene, HelixState& hx)
	{
		hx.objBase = (int)scene.voxelObjects.size();

		scene.voxelObjects.push_back(VoxelObject(1, 1, 1, { PAL_HX_REST }));
		scene.voxelObjects.push_back(VoxelObject(1, 1, 1, { PAL_HX_PULSE }));
		scene.voxelObjects.push_back(VoxelObject(1, 1, 1, { PAL_HX_FLASH }));

		{
			Material m;
			m.type = MaterialType::Lambertian;
			m.albedo = float3(0.96f, 0.955f, 0.95f);
			m.roughness = 0.25f;
			scene.materials[MAT_COUNT + (PAL_HX_REST - 1)] = m;
		}
		{
			Material m;
			m.type = MaterialType::Lambertian;
			m.albedo = float3(0.88f, 0.93f, 0.91f);
			m.roughness = 0.18f;
			scene.materials[MAT_COUNT + (PAL_HX_PULSE - 1)] = m;
		}
		{
			Material m;
			m.type = MaterialType::Emissive;
			m.albedo = float3(0.92f, 0.96f, 0.94f);
			m.emission = float3(0.549f, 0.722f, 0.690f);
			m.emissionStr = 1.5f;
			m.roughness = 0.12f;
			scene.materials[MAT_COUNT + (PAL_HX_FLASH - 1)] = m;
		}
	}


	inline void SyncHelix(
		const HelixState& hx,
		Tmpl8::Scene& scene,
		float3 rotRad)
	{
		scene.voxelInstances.clear();

		int maxRung = std::min((int)ceilf(hx.rungReveal), HX_MAX_RUNGS);
		if (maxRung <= 0) { scene.RebuildDirtyInstances(); return; }

		const float3 center(256, 240, 256);
		float3 groupRotDeg = rotRad * HX_RAD2DEG;

		mat4 groupRot = mat4::RotateX(rotRad.x)
			* mat4::RotateY(rotRad.y)
			* mat4::RotateZ(rotRad.z);

		auto xform = [&](float3 p) -> float3
			{
				float3 o = p - center;
				float4 r = groupRot * float4(o, 1);
				return float3(r.x, r.y, r.z) + center;
			};

		for (int i = 0; i < maxRung; i++)
		{
			float alpha = hx.GetRungAlpha(i);
			if (alpha < 0.01f) continue;

			float t = (float)i / (float)(HX_MAX_RUNGS - 1);

			float r = hx.GetRadius(t);
			float y = center.y - hx.height * 0.5f + t * hx.height;
			float theta = t * hx.twist * 2.0f * PI + hx.helixSpin;

			float scan = hx.GetScanAmp(t);

			for (int strand = 0; strand < hx.strandCount; strand++)
			{
				float sFade = (strand == 1) ? hx.strand2Fade : 1.0f;
				float combined = alpha * sFade;
				if (combined < 0.01f) continue;

				float sAngle = theta + strand * PI;

				float3 pos(
					center.x + cosf(sAngle) * r,
					y,
					center.z + sinf(sAngle) * r
				);

				pos = pos + hx.GetMicroDrift(strand, t);

				float3 cubeRot = hx.GetCubeRot(strand, t);
				float3 totalRotDeg = groupRotDeg + cubeRot;

				float scale = hx.cubeSize * hsmooth(combined);

				// scan proximity also slightly scales up cubes
				scale *= 1.0f + scan * 0.08f;

				int objIdx;
				if (scan > 0.6f)
					objIdx = hx.objBase + 2;
				else if (scan > 0.15f)
					objIdx = hx.objBase + 1;
				else
					objIdx = hx.objBase + 0;

				if (scale < 0.1f) continue;

				VoxelFactory::CreateInstance(
					scene, objIdx,
					xform(pos), totalRotDeg,
					float3(scale)
				);
			}
		}

		scene.RebuildDirtyInstances();
	}


	inline SceneDef HelixShowcase()
	{
		SceneDef s;
		s.name = "Helix";
		s.useVoxelGrid = true;

		s.voxObjects.push_back({
			"assets/Showcase/display_platform.vox",
			float3(256, 120, 256), float3(0), float3(1), true
			});

		s.camPos = float3(0.28f, 0.60f, 0.20f);
		s.camTarget = float3(0.50f, 0.47f, 0.50f);

		s.splineSpeed = 0.08f;

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

		s.sky.sunDir = normalize(float3(0.2f, -0.6f, 0.15f));
		s.sky.sunColor = float3(1.0f, 0.99f, 0.97f);
		s.sky.sunIntensity = 1.8f;
		s.sky.timeOfDay = 0.32f;
		s.sky.animate = false;

		auto addL = [&](float3 p, float3 c)
			{
				PointLight l;
				l.position = p; l.color = c; l.enabled = true;
				s.pointLights.push_back(l);
			};
		addL(float3(0.50f, 0.92f, 0.50f), float3(0.85f, 0.84f, 0.83f));
		addL(float3(0.20f, 0.50f, 0.20f), float3(0.35f, 0.35f, 0.38f));
		addL(float3(0.80f, 0.50f, 0.80f), float3(0.35f, 0.35f, 0.38f));
		addL(float3(0.50f, 0.30f, 0.50f), float3(0.25f, 0.25f, 0.27f));

		auto& hx = GetHelixState();
		hx = std::make_shared<HelixState>();
		hx->Reset();  // seeds initial values

		s.tickCallback = [hx](SceneDef&, Tmpl8::Scene& scene,
			float dt, std::function<void()> reset)
			{
				scene.instancesShadows = false;
				if (hx->needsInit)
				{
					hx->needsInit = false;
					SetupHelixCubes(scene, *hx);
					SyncHelix(*hx, scene, hx->GetRot());
					if (reset) reset();
					return;
				}

				hx->Tick(dt);
				SyncHelix(*hx, scene, hx->GetRot());
			};

		s.uiCallback = [hx](SceneDef&, Tmpl8::Scene& scene,
			std::function<void()> reset)
			{
				if (!ImGui::CollapsingHeader("Helix", ImGuiTreeNodeFlags_DefaultOpen))
					return;

				float secs = hx->progress * HX_DURATION;
				ImGui::Text("%s  |  %.1fs / %.1fs",
					hx->GetPhaseLabel(), secs, HX_DURATION);
				ImGui::ProgressBar(hx->progress, ImVec2(-1, 4), "");

				ImGui::Spacing();

				ImGui::Text("%.0f rungs  |  %d strand%s  |  %d inst",
					hx->rungReveal,
					hx->strandCount, hx->strandCount > 1 ? "s" : "",
					(int)scene.voxelInstances.size());

				if (hx->scanDnActive)
					ImGui::Text("Dual scan active");

				ImGui::Spacing(); ImGui::Separator();

				if (ImGui::Button(hx->paused ? "  Resume  " : "  Pause   "))
					hx->paused = !hx->paused;
				ImGui::SameLine();
				if (ImGui::Button("Reset"))
				{
					hx->Reset();
					SyncHelix(*hx, scene, hx->GetRot());
					if (reset) reset();
				}
			};

		return s;
	}

}