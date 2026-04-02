#pragma once
#include "camera.h"
#include "Sky.h"
#include "Core/Lighting/AreaLight.h"
#include "Core/Lighting/PointLight.h"
#include "Core/Lighting/SpotLight.h"
#include "Core/Animations/SplineFollower.h"
#include "PhysicsWorld.h"
#include "SceneManager.h"
#include "GameScenes.h"
#include "EventSystem.h"

void RunAllTests();


class material;

struct SceneLights
{
	std::vector<PointLight> points;
	std::vector<DirectionalLight> directionals;
	std::vector<SpotLight> spots;
	std::vector<AreaLight> areas;
};

namespace Tmpl8
{
	class Renderer : public TheApp
	{
	public:
		/// <summary>Returns squared vector length.</summary>
		static inline float length2(const float3& v)
		{
			return v.x * v.x + v.y * v.y + v.z * v.z;
		}
		/// <summary>Samples a random point inside the unit sphere.</summary>
		static inline float3 RandomInUnitSphere()
		{
			float3 p;
			do { p = 2.0f * float3(RandomFloat(), RandomFloat(), RandomFloat()) - float3(1, 1, 1); } while (length2(p) >= 1.0f);
			return p;
		}
		/// <summary>Reflects a vector around a surface normal.</summary>
		static inline float3 reflect(const float3& v, const float3& n)
		{
			return v - 2.0f * dot(v, n) * n;
		}
		/// <summary>Computes a refracted direction using Snell's law.</summary>
		static inline bool Refract(const float3& v, const float3& n, const float ni_over_nt, float3& refracted)
		{
			const float3 uv = normalize(v);
			const float dt = dot(uv, n);
			const float discriminant = 1.0f - ni_over_nt * ni_over_nt * (1 - dt * dt);
			if (discriminant > 0) {
				refracted = ni_over_nt * (uv - n * dt) - n * sqrt(discriminant);
				return true;
			}
			return false;
		}
		/// <summary>Approximates Fresnel reflectance with Schlick's model.</summary>
		static inline float Schlick(const float cosine, const float ref_idx)
		{
			float r0 = (1 - ref_idx) / (1 + ref_idx);
			r0 = r0 * r0;
			return r0 + (1 - r0) * powf(1 - cosine, 5);
		}
		/// <summary>Returns a blue-noise sample for pixel and frame indices.</summary>
		float BlueNoise(const int x, const int y, const int frame) const
		{
			// Bitmask wraps coordinates because BN_SIZE is a power of two.
			const int ix = (x + frame * 17) & (BN_SIZE - 1);
			const int iy = (y + frame * 31) & (BN_SIZE - 1);
			return blueNoise[ix + iy * BN_SIZE] * (1.0f / 255.0f);
		}

		bool rebuildSphereBVH = false;
		float lastFrameTime = 0.0f;
		float avgFrameTimeMs = 0.0f;
		float fps = 0.0f;
		float rps = 0.0f;

		/// <summary>Initializes renderer state and resources.</summary>
		void Init();
		/// <summary>Traces a ray and returns the shaded radiance.</summary>
		float3 Trace(Ray& ray, int = 0, int = 0, int = 0);
		/// <summary>Advances one frame of rendering and simulation.</summary>
		void Tick(float deltaTime);
		/// <summary>Draws frame statistics user interface.</summary>
		void UIStats();
		/// <summary>Draws main renderer user interface.</summary>
		void UI();
		/// <summary>Draws lighting controls user interface.</summary>
		void LightUI() const;
		/// <summary>Draws material controls and returns whether values changed.</summary>
		static bool MaterialUI(const char* label, Material& material);
		/// <summary>Releases renderer-owned resources.</summary>
		void Shutdown();
		void MouseUp(int button) { button = 0; }
		/// <summary>Handles mouse press events.</summary>
		void MouseDown(int button);
		void MouseMove(int x, int y)
		{
#if defined(DOUBLESIZE) && !defined(FULLSCREEN)
			mousePos.x = x / 2, mousePos.y = y / 2;
#else
			mousePos.x = x, mousePos.y = y;
#endif
		}
		void MouseWheel(float y) { y = 0; }
		void KeyUp(int key) { key = 0; }
		/// <summary>Handles key press events.</summary>
		void KeyDown(int key);

		int2 mousePos;
		float3* accumulator = nullptr;
		float3* history = nullptr;
		Scene scene;
		Camera camera;
		Camera prevCamera;
		Frustum previousFrustum;
		int* sampleCountPerPixel = nullptr;
		SceneLights lights;

		bool debugNormals = false;
		bool fastSphereShading = true;
		int fastSphereThreshold = 1000;
		float3 fastSphereLightDir = normalize(float3(0.5f, 0.8f, 0.3f));
		static constexpr float fastSphereAmbient = 0.12f;

		float3* rayDirTable = nullptr;
		bool    rayTableDirty = true;
		uint32_t sampleCount = 0;
		uint32_t frameIndex = 0;
		mat4 lastViewMatrix;

		/// <summary>Allocates accumulation buffers for progressive rendering.</summary>
		void InitAccumulator();
		/// <summary>Resets progressive accumulation state.</summary>
		void ResetAccumulator();

		int selectedMaterialIndex = -1;
		bool selectionLocked = false;
		bool editingMaterial = false;

		static constexpr int BN_SIZE = 256;
		uint8_t* blueNoise = nullptr;

		Sky sky;
		CatmullRomSpline cameraSpline;
		SplineFollower cameraFollower;
		bool useSplineCamera = false;

		PhysicsWorld physics;
		SceneManager sceneManager;
		int lastLoadedSceneID = -1;

		EventSystem eventSystem;

		float3* bloomDown = nullptr;
		float3* bloomTemp = nullptr;
		static constexpr int BLOOM_W = SCRWIDTH / 4;
		static constexpr int BLOOM_H = SCRHEIGHT / 4;
		bool   enableBloom = true;
		float  bloomThreshold = 1.0f;
		float  bloomIntensity = 0.35f;
		int    bloomRadius = 6;
		/// <summary>Applies bloom post-processing to the render output.</summary>
		void   ApplyBloom() const;

		float  fadeOpacity = 1.0f;
		float  fadeTarget = 0.0f;
		float  fadeSpeed = 1.0f;
		float3 fadeColor = float3(0, 0, 0);

		bool  lightFadeActive = false;
		float lightFadeTimer = 0.0f;
		float lightFadeDuration = 2.0f;
		float lightFadeFrom = 0.0f;
		float lightFadeTo = 1.0f;
		float lightFadeMult = 1.0f;

		std::vector<float3> originalPointLightColors;
		bool lightColorsStored = false;


		bool  bloomFadeActive = false;
		float bloomFadeTimer = 0.0f;
		float bloomFadeDuration = 1.0f;
		float bloomIntensityFrom = 0.35f;
		float bloomIntensityTo = 0.35f;
		float bloomThresholdFrom = 1.0f;
		float bloomThresholdTo = 1.0f;
	};
}
