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

	static constexpr float GYRO_BPM = 79.0f;
	static constexpr float GYRO_BEAT = 60.0f / GYRO_BPM;
	static constexpr float GYRO_CAM_SPEED = 0.15f;

	enum GyroPhase
	{
		GYRO_ORDER = 0,
		GYRO_CONVERGE,
		GYRO_LOCK,
		GYRO_SPINOUT,
		GYRO_FREEZE,
		GYRO_COLLAPSE,
		GYRO_GATHER,
		GYRO_TRAIL,
		GYRO_PHASE_COUNT
	};


	// --- Catmull-Rom helpers ---

	inline float3 GyroCatmullRom(const float3 p0, const float3 p1, const float3 p2, const float3 p3, const float t)
	{
		float t2 = t * t, t3 = t2 * t;
		return 0.5f * (
			(2.0f * p1) +
			(-p0 + p2) * t +
			(2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
			(-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3
			);
	}

	inline float3 GyroEvalSpline(const float3* pts, const int nPts, const float t01)
	{
		int segs = nPts - 3;
		if (segs < 1) return pts[0];
		float s = t01 * segs;
		int i = (int)s;
		if (i >= segs) i = segs - 1;
		if (i < 0) i = 0;
		float local = s - (float)i;
		return GyroCatmullRom(pts[i], pts[i + 1], pts[i + 2], pts[i + 3], local);
	}

	// --- Arc-length camera position estimator ---

	inline float3 GyroEstimateCamPos(
		const std::vector<float3>& splinePts,
		const float elapsedTime)
	{
		if (splinePts.size() < 4) return float3(0.5f, 0.5f, 0.25f);

		const int N = 256;
		float3 samples[257];
		float cumLen[257];
		cumLen[0] = 0;

		for (int i = 0; i <= N; i++)
		{
			float t = (float)i / (float)N;
			samples[i] = GyroEvalSpline(splinePts.data(), (int)splinePts.size(), t);
			if (i > 0)
				cumLen[i] = cumLen[i - 1] + length(samples[i] - samples[i - 1]);
		}

		float totalLen = cumLen[N];
		if (totalLen < 0.001f) return samples[0];

		float dist = fmodf(elapsedTime * GYRO_CAM_SPEED, totalLen);
		if (dist < 0) dist += totalLen;

		// binary search
		int lo = 0, hi = N;
		while (lo < hi)
		{
			int mid = (lo + hi) / 2;
			if (cumLen[mid] < dist) lo = mid + 1;
			else hi = mid;
		}
		if (lo == 0) return samples[0];

		float segLen = cumLen[lo] - cumLen[lo - 1];
		// Interpolate inside the located arc-length segment for near-constant camera speed.
		float frac = (segLen > 0.0001f) ? (dist - cumLen[lo - 1]) / segLen : 0.0f;
		return samples[lo - 1] * (1.0f - frac) + samples[lo] * frac;
	}


	// --- State ---

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

		Ring rings[NUM_RINGS] = {
			{  0.00f,       0.00f,       0.30f,  167, 0.42f, 0 },
			{  0.00f,       0.00f,      -0.30f,  167, 0.38f, 0 },
			{  PI * 0.5f,   0.00f,       0.25f,  167, 0.36f, 1 },
			{  PI * 0.5f,   0.00f,      -0.25f,  167, 0.32f, 1 },
			{  0.00f,       PI * 0.5f,   0.35f,  167, 0.35f, 2 },
			{  0.00f,       PI * 0.5f,  -0.35f,  167, 0.31f, 2 },
		};

		// camera override for trail
		float3 trailCamFixedPos = float3(0.5f, 0.48f, 0.15f);
		float3 trailCamFixedTarget = float3(0.5f, 0.45f, 0.5f);
		float3 camCapturedPos;
		float3 camCapturedTarget;
		bool   camCaptured = false;
		float  camBlend = 0.0f; // 0 = spline, 1 = fixed
		float  savedSplineSpeed = 0.15f;

		float gimbalPrecession[3] = { 0.12f, 0.10f, 0.14f };

		// beat-synced timing
		float phaseBeats[GYRO_PHASE_COUNT] = { 4, 2, 4, 4, 8, 2, 4, 20 };
		int   cyclePreset = 48;

		float time = 0.0f;
		float cycleTime = 0.0f;
		bool  initialized = false;
		float flickerPhase = 0.0f;
		GyroPhase phase = GYRO_ORDER;

		// trail camera basis (stored when trail is built)
		float3 trailCamFwd;
		float3 trailCamRight;
		float3 trailCamUp;
		float3 trailCamPos;

		float orbitSphereRadius = 0.005f;
		float centerSphereRadius = 0.14f;

		float globalSpeed = 1.0f;
		float precessionSpeed = 1.0f;

		// explosion
		std::vector<float3> chaosVelocity;
		std::vector<float3> chaosOffset;
		bool chaosInitialized = false;
		float collapseTime = 0.0f;

		// animation drives
		float animOrbitMul = 1.0f;
		float animPrecessMul = 1.0f;
		float animLockBlend = 0.0f;
		float animPanicWobble = 0.0f;
		float animRadiusDecay = 1.0f;

		std::vector<uint> sphereMaterials;

		// gather / trail
		std::vector<float3> snapshotPos;
		bool  gatherCaptured = false;
		bool  useOverridePos = false;
		std::vector<float3> overridePos;
		float gatherTime = 0.0f;
		float trailTime = 0.0f;
		bool  trailBuilt = false;

		// trail spline (built dynamically from camera position)
		static constexpr int TRAIL_PTS = 7;
		float3 trailSpline[TRAIL_PTS];


		// --- helpers ---

		int NumOrbitSpheres() const
		{
			int n = 0;
			for (int i = 0; i < NUM_RINGS; i++) n += rings[i].count;
			return n;
		}
		int TotalSpheres() const { return NumOrbitSpheres() + 1; }

		float PhaseDuration(const int p) const { return phaseBeats[p] * GYRO_BEAT; }

		float PhaseStart(const int p) const
		{
			float t = 0;
			for (int i = 0; i < p; i++) t += phaseBeats[i] * GYRO_BEAT;
			return t;
		}

		float TotalCycleSec() const
		{
			float t = 0;
			for (int i = 0; i < GYRO_PHASE_COUNT; i++) t += phaseBeats[i];
			return t * GYRO_BEAT;
		}

		int TotalCycleBeats() const
		{
			int t = 0;
			for (int i = 0; i < GYRO_PHASE_COUNT; i++) t += (int)phaseBeats[i];
			return t;
		}

		void ApplyPreset(const int beats)
		{
			cyclePreset = beats;
			if (beats == 32)
			{
				//              ORDER CONV LOCK SPIN FREEZE COLL GATHER TRAIL
				phaseBeats[0] = 4;    // 1 bar
				phaseBeats[1] = 2;    // half bar
				phaseBeats[2] = 2;    // half bar
				phaseBeats[3] = 2;    // half bar
				phaseBeats[4] = 4;    // 1 bar
				phaseBeats[5] = 2;    // half bar
				phaseBeats[6] = 4;    // 1 bar
				phaseBeats[7] = 12;   // 3 bars
			}
			else // 48
			{
				phaseBeats[0] = 4;    // 1 bar
				phaseBeats[1] = 2;    // half bar
				phaseBeats[2] = 4;    // 1 bar
				phaseBeats[3] = 4;    // 1 bar
				phaseBeats[4] = 8;    // 2 bars
				phaseBeats[5] = 2;    // half bar
				phaseBeats[6] = 4;    // 1 bar
				phaseBeats[7] = 20;   // 5 bars
			}
		}

		void AssignMaterials()
		{
			sphereMaterials.clear();
			int total = NumOrbitSpheres();
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
			int total = TotalSpheres();
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
				chaosOffset[i] = float3(0);
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

		void ResetTrail()
		{
			snapshotPos.clear();
			overridePos.clear();
			gatherCaptured = false;
			useOverridePos = false;
			trailBuilt = false;
			gatherTime = 0.0f;
			trailTime = 0.0f;
			camCaptured = false;
			camBlend = 0.0f;
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
			ResetTrail();
		}

		// capture positions at start of GATHER
		void CaptureGather(const Tmpl8::Scene& scene,
			const std::vector<float3>& camSplinePts, float elapsedTime,
			float currentSplineSpeed)
		{
			if (gatherCaptured) return;
			gatherCaptured = true;
			gatherTime = 0.0f;
			snapshotPos.clear();
			for (const auto& sp : scene.spheres)
				snapshotPos.push_back(sp.center);
			useOverridePos = true;
			overridePos = snapshotPos;

			// capture current camera position for smooth lerp
			camCapturedPos = GyroEstimateCamPos(camSplinePts, elapsedTime);
			camCapturedTarget = float3(0.5f, 0.45f, 0.5f);
			camCaptured = true;
			camBlend = 0.0f;
			savedSplineSpeed = currentSplineSpeed;
		}

		void UpdateGather(float dt)
		{
			gatherTime += dt;
			float dur = PhaseDuration(GYRO_GATHER);
			float p = std::min(gatherTime / dur, 1.0f);
			float ep = p * p * (3.0f - 2.0f * p);

			// lerp spheres to center
			float3 target(0.5f, 0.5f, 0.5f);
			int total = (int)snapshotPos.size();
			overridePos.resize(total);
			for (int i = 0; i < total; i++)
				overridePos[i] = snapshotPos[i] * (1.0f - ep) + target * ep;

			// lerp camera to fixed trail position
			camBlend = ep;
		}
		void BuildTrailSpline(const std::vector<float3>& camSplinePts, float trailStartTime)
		{
			if (trailBuilt) return;
			trailBuilt = true;

			float3 camPos = trailCamFixedPos;
			float3 camFwd = normalize(trailCamFixedTarget - camPos);
			float3 worldUp(0, 1, 0);
			float3 camRight = normalize(cross(camFwd, worldUp));
			float3 camUp = cross(camRight, camFwd);

			float3 gather(0.5f, 0.5f, 0.5f);

			// pass 0.03 units in front of camera  at 120 FOV screen half-width = 0.052
			float passDist = 0.03f;
			float sweep = 0.15f; // how far right/left the sweep extends

			// spline sweeps from right to left ACROSS the camera view
			// P0: tangent control behind gather
			trailSpline[0] = gather + (gather - camPos) * 0.15f;
			// P1: gather point (start)
			trailSpline[1] = gather;
			// P2: approach  come in from the right side
			trailSpline[2] = camPos + camFwd * passDist * 3.0f + camRight * sweep * 1.5f;
			// P3: wipe entry  right edge of screen, close to camera
			trailSpline[3] = camPos + camFwd * passDist + camRight * sweep * 0.3f;
			// P4: wipe exit  left edge of screen, still close
			trailSpline[4] = camPos + camFwd * passDist - camRight * sweep * 0.3f;
			// P5: depart left
			trailSpline[5] = camPos + camFwd * passDist * 3.0f - camRight * sweep * 1.5f;
			// P6: tangent exit
			trailSpline[6] = camPos + camFwd * passDist * 4.0f - camRight * sweep * 2.5f;

			for (int i = 0; i < TRAIL_PTS; i++)
			{
				trailSpline[i].x = std::clamp(trailSpline[i].x, 0.02f, 0.98f);
				trailSpline[i].y = std::clamp(trailSpline[i].y, 0.02f, 0.98f);
				trailSpline[i].z = std::clamp(trailSpline[i].z, 0.02f, 0.98f);
			}

			trailCamFwd = camFwd;
			trailCamRight = camRight;
			trailCamUp = camUp;
			trailCamPos = camPos;
		}

		void UpdateTrail(float dt)
		{
			trailTime += dt;
			float dur = PhaseDuration(GYRO_TRAIL);
			int total = TotalSpheres();
			int numOrbit = NumOrbitSpheres();

			float traverseTime = dur * 0.20f;
			float launchWindow = dur * 0.40f;
			float stagger = launchWindow / (float)total;

			overridePos.resize(total);

			// at 120 FOV, d=0.03: screen half-width = 0.03 * tan(60) = 0.052
			// ribbon needs radius ~0.06 to fully cover

			for (int i = 0; i < total; i++)
			{
				int launchOrder;
				if (i == numOrbit) launchOrder = 0;
				else               launchOrder = i + 1;

				float startTime = launchOrder * stagger;
				float localTime = trailTime - startTime;

				if (localTime <= 0.0f)
				{
					overridePos[i] = trailSpline[1];
					continue;
				}

				float rawT = std::min(localTime / traverseTime, 1.0f);
				float tEased = rawT * rawT * (3.0f - 2.0f * rawT);

				float3 pos = GyroEvalSpline(trailSpline, TRAIL_PTS, tEased);

				// tangent
				float tAhead = std::min(tEased + 0.01f, 1.0f);
				float3 ahead = GyroEvalSpline(trailSpline, TRAIL_PTS, tAhead);
				float3 tang = ahead - pos;
				float tLen = length(tang);
				if (tLen > 0.0001f) tang = tang / tLen;
				else                tang = float3(1, 0, 0);

				float3 right = normalize(cross(tang, float3(0, 1, 0)));
				float3 up = cross(right, tang);

				// distance to camera drives ribbon thickness
				float distToCam = length(pos - trailCamPos);

				// expand ribbon when close to camera to fill screen
				float wipeZone = 1.0f - std::min(distToCam / 0.10f, 1.0f);
				wipeZone = wipeZone * wipeZone;

				// thin stream (0.006) -> screen-filling wall (0.065) near camera
				float ribbonRadius = 0.006f + wipeZone * 0.065f;

				float angle1 = launchOrder * 0.37f;
				float angle2 = launchOrder * 0.53f;
				pos = pos + right * sinf(angle1) * ribbonRadius + up * cosf(angle2) * ribbonRadius;

				overridePos[i] = pos;
			}
		}
	};


	// --- Materials ---

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

	inline float gyroSmoothstep(float a, float b, float t)
	{
		float x = (t - a) / (b - a);
		x = x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x);
		return x * x * (3.0f - 2.0f * x);
	}


	// --- Sphere rebuild ---

	inline void RebuildGyroscopeSpheres(
		Tmpl8::Scene& scene,
		GyroState& state)
	{
		scene.spheres.clear();

		int numOrbit = state.NumOrbitSpheres();

		// --- override path (GATHER / TRAIL) ---
		if (state.useOverridePos && (int)state.overridePos.size() >= numOrbit + 1)
		{
			float cRadius = state.centerSphereRadius;
			if (state.phase == GYRO_GATHER)
			{
				float dur = state.PhaseDuration(GYRO_GATHER);
				float p = std::min(state.gatherTime / dur, 1.0f);
				cRadius = state.centerSphereRadius * (1.0f - p) + state.orbitSphereRadius * 3.0f * p;
			}
			else if (state.phase == GYRO_TRAIL)
			{
				cRadius = state.orbitSphereRadius * 3.0f;
			}

			for (int i = 0; i < numOrbit; i++)
			{
				scene.spheres.push_back({
					state.overridePos[i],
					state.orbitSphereRadius,
					state.sphereMaterials[i]
					});
			}
			scene.spheres.push_back({
				state.overridePos[numOrbit],
				cRadius,
				MAT_GYRO_CENTER
				});

			scene.BuildSphereBVH();
			return;
		}

		// --- orbital path (ORDER through COLLAPSE) ---
		const float3 center(0.5f, 0.5f, 0.5f);
		const float t = state.time;
		int sphereIdx = 0;

		float aY = t * state.gimbalPrecession[0] * state.precessionSpeed * state.animPrecessMul;
		float aX = t * state.gimbalPrecession[1] * state.precessionSpeed * state.animPrecessMul;
		float aZ = t * state.gimbalPrecession[2] * state.precessionSpeed * state.animPrecessMul;

		if (state.animLockBlend > 0.0f)
		{
			aX = aX * (1.0f - state.animLockBlend);
			aZ = aZ * (1.0f - state.animLockBlend);
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


	// --- Scene definition ---

	inline SceneDef GyroscopeShowcase()
	{
		SceneDef s;
		s.name = "Gyroscope";
		s.useVoxelGrid = false;

		s.camPos = float3(0.5f, 0.46f, 0.25f);
		s.camTarget = float3(0.5f, 0.45f, 0.5f);

		s.splineSpeed = 0.15f;

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
		gyro->ApplyPreset(48);

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

				float totalSec = gyro->TotalCycleSec();
				if (ct >= totalSec)
				{
					gyro->ResetCycle();
					ct = 0.0f;
				}

				float tConv = gyro->PhaseStart(GYRO_CONVERGE);
				float tLock = gyro->PhaseStart(GYRO_LOCK);
				float tSpinout = gyro->PhaseStart(GYRO_SPINOUT);
				float tFreeze = gyro->PhaseStart(GYRO_FREEZE);
				float tCollapse = gyro->PhaseStart(GYRO_COLLAPSE);
				float tGather = gyro->PhaseStart(GYRO_GATHER);
				float tTrail = gyro->PhaseStart(GYRO_TRAIL);

				if (ct < tConv)
				{
					gyro->phase = GYRO_ORDER;
					gyro->animOrbitMul = 1.5f;
					gyro->animPrecessMul = 1.0f;
					gyro->animLockBlend = 0.0f;
					gyro->animPanicWobble = 0.0f;
					gyro->animRadiusDecay = 1.0f;
				}
				else if (ct < tLock)
				{
					gyro->phase = GYRO_CONVERGE;
					float p = (ct - tConv) / (tLock - tConv);
					gyro->animLockBlend = gyroSmoothstep(0.0f, 1.0f, p);
					gyro->animOrbitMul = 1.5f + 0.5f * p;
					gyro->animPrecessMul = 1.0f + 0.5f * p;
				}
				else if (ct < tSpinout)
				{
					gyro->phase = GYRO_LOCK;
					float p = (ct - tLock) / (tSpinout - tLock);
					gyro->animLockBlend = 1.0f;
					gyro->animPanicWobble = 0.3f + 0.7f * p;
					gyro->animOrbitMul = 2.0f + 3.0f * p;
					gyro->animPrecessMul = 1.5f + 2.0f * p;
				}
				else if (ct < tFreeze)
				{
					gyro->phase = GYRO_SPINOUT;
					float p = (ct - tSpinout) / (tFreeze - tSpinout);
					gyro->animLockBlend = 1.0f - 0.3f * p;
					gyro->animPanicWobble = 1.0f + 1.5f * p;
					gyro->animOrbitMul = 5.0f + 10.0f * p;
					gyro->animPrecessMul = 3.5f + 5.0f * p;
				}
				else if (ct < tCollapse)
				{
					gyro->phase = GYRO_FREEZE;
					float p = (ct - tFreeze) / (tCollapse - tFreeze);
					float brake = 1.0f - gyroSmoothstep(0.0f, 1.0f, p);
					gyro->animOrbitMul = 15.0f * brake;
					gyro->animPrecessMul = 8.5f * brake;
					gyro->animPanicWobble = 2.5f * brake;
					gyro->animLockBlend = 0.7f * brake;
				}
				else if (ct < tGather)
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
				else if (ct < tTrail)
				{
					gyro->phase = GYRO_GATHER;
					gyro->CaptureGather(scene, def.splinePoints, gyro->time, def.splineSpeed);
					gyro->UpdateGather(dt);
				}
				else
				{
					gyro->phase = GYRO_TRAIL;
					if (!gyro->trailBuilt)
					{
						if (!gyro->useOverridePos)
						{
							gyro->CaptureGather(scene, def.splinePoints, gyro->time, def.splineSpeed);
							gyro->gatherTime = gyro->PhaseDuration(GYRO_GATHER);
						}
						gyro->camBlend = 1.0f;
						gyro->BuildTrailSpline(def.splinePoints, gyro->time);
					}
					gyro->UpdateTrail(dt);
				}

				// --- camera lock during GATHER and TRAIL ---
				if (gyro->camCaptured && gyro->camBlend > 0.001f)
				{
					float b = gyro->camBlend;
					float3 pos = gyro->camCapturedPos * (1.0f - b) + gyro->trailCamFixedPos * b;
					float3 tgt = gyro->camCapturedTarget * (1.0f - b) + gyro->trailCamFixedTarget * b;

					// disable spline, override camera directly
					def.splineEnabled = false;
					def.camPos = pos;
					def.camTarget = tgt;
				}
				else
				{
					def.splineEnabled = true;
				}

				// --- center sphere emission ---
				gyro->flickerPhase += dt;
				float beatPhase = fmodf(gyro->cycleTime, GYRO_BEAT) / GYRO_BEAT;
				float basePulse = 0.4f + 0.6f * powf(
					0.5f * (1.0f + sinf(beatPhase * 2.0f * PI)), 2.0f);

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
					float collapseDur = gyro->PhaseDuration(GYRO_COLLAPSE);
					float fade = 1.0f - gyro->collapseTime / collapseDur;
					flickerMul = max(0.0f, fade * 0.5f);
				}
				else if (gyro->phase == GYRO_GATHER)
				{
					float dur = gyro->PhaseDuration(GYRO_GATHER);
					float p = std::min(gyro->gatherTime / dur, 1.0f);
					flickerMul = 0.1f + 0.9f * p;
				}
				else if (gyro->phase == GYRO_TRAIL)
				{
					flickerMul = 0.6f + 0.4f * basePulse;
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
					"ORDER", "CONVERGE", "GIMBAL LOCK", "SPIN OUT",
					"FREEZE", "COLLAPSE", "GATHER", "TRAIL"
				};
				ImVec4 phaseColor = ImVec4(0.3f, 0.8f, 0.3f, 1.0f);
				if (gyro->phase == GYRO_CONVERGE) phaseColor = ImVec4(0.9f, 0.7f, 0.2f, 1.0f);
				if (gyro->phase == GYRO_LOCK)     phaseColor = ImVec4(0.9f, 0.3f, 0.1f, 1.0f);
				if (gyro->phase == GYRO_SPINOUT)  phaseColor = ImVec4(1.0f, 0.1f, 0.1f, 1.0f);
				if (gyro->phase == GYRO_FREEZE)   phaseColor = ImVec4(0.5f, 0.5f, 0.8f, 1.0f);
				if (gyro->phase == GYRO_COLLAPSE) phaseColor = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);
				if (gyro->phase == GYRO_GATHER)   phaseColor = ImVec4(0.6f, 0.8f, 0.9f, 1.0f);
				if (gyro->phase == GYRO_TRAIL)    phaseColor = ImVec4(0.8f, 0.6f, 0.9f, 1.0f);

				ImGui::TextColored(phaseColor, "Phase: %s", phaseNames[gyro->phase]);
				ImGui::SameLine();
				ImGui::TextDisabled("%.1f / %.1fs", gyro->cycleTime, gyro->TotalCycleSec());

				ImGui::ProgressBar(gyro->cycleTime / gyro->TotalCycleSec(), ImVec2(-1, 3));

				int totalBeats = gyro->TotalCycleBeats();
				float currentBeat = gyro->cycleTime / GYRO_BEAT;
				ImGui::Text("%d spheres  |  beat %.1f / %d  |  %.1fs",
					(int)scene.spheres.size(), currentBeat, totalBeats, gyro->TotalCycleSec());

				ImGui::Spacing();

				if (ImGui::Button("32 beats (8 bar)"))
				{
					gyro->ApplyPreset(32);
					gyro->ResetCycle();
					RebuildGyroscopeSpheres(scene, *gyro);
					if (resetAcc) resetAcc();
				}
				ImGui::SameLine();
				if (ImGui::Button("48 beats (12 bar)"))
				{
					gyro->ApplyPreset(48);
					gyro->ResetCycle();
					RebuildGyroscopeSpheres(scene, *gyro);
					if (resetAcc) resetAcc();
				}

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

				if (ImGui::CollapsingHeader("Timing (beats)"))
				{
					ImGui::Text("Cycle: %d beats (%.1fs)", totalBeats, gyro->TotalCycleSec());
					static const char* pNames[] = {
						"Order##gy", "Converge##gy", "Lock##gy", "Spinout##gy",
						"Freeze##gy", "Collapse##gy", "Gather##gy", "Trail##gy"
					};
					for (int i = 0; i < GYRO_PHASE_COUNT; i++)
						ImGui::SliderFloat(pNames[i], &gyro->phaseBeats[i], 0, 32, "%.0f");
				}

				if (ImGui::TreeNode("Animation State"))
				{
					ImGui::TextDisabled("Orbit mul:    %.2f", gyro->animOrbitMul);
					ImGui::TextDisabled("Precess mul:  %.2f", gyro->animPrecessMul);
					ImGui::TextDisabled("Lock blend:   %.2f", gyro->animLockBlend);
					ImGui::TextDisabled("Panic wobble: %.2f", gyro->animPanicWobble);
					ImGui::TextDisabled("Radius decay: %.2f", gyro->animRadiusDecay);
					if (gyro->phase == GYRO_GATHER)
						ImGui::TextDisabled("Gather:       %.2f", gyro->gatherTime);
					if (gyro->phase == GYRO_TRAIL)
						ImGui::TextDisabled("Trail:        %.2f / %.2f", gyro->trailTime, gyro->PhaseDuration(GYRO_TRAIL));
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