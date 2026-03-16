#pragma once
#include "camera.h"
#include "Sky.h"
#include "Core/Lighting/AreaLight.h"
#include "Core/Lighting/PointLight.h"
#include "Core/Lighting/SpotLight.h"
#include "Core/Animations/SplineFollower.h"
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
		inline float length2(const float3& v) {
			return v.x * v.x + v.y * v.y + v.z * v.z;
		}
		inline float3 RandomInUnitSphere() {
			float3 p;
			do { p = 2.0f * float3(RandomFloat(), RandomFloat(), RandomFloat()) - float3(1, 1, 1); } while (length2(p) >= 1.0f);
			return p;
		}
		inline float3 reflect(const float3& v, const float3& n)
		{
			return v - 2.0f * dot(v, n) * n;
		}
		inline bool Refract(const float3& v, const float3& n, float ni_over_nt, float3& refracted)
		{
			float3 uv = normalize(v);
			float dt = dot(uv, n);
			float discriminant = 1.0f - ni_over_nt * ni_over_nt * (1 - dt * dt);
			if (discriminant > 0) {
				refracted = ni_over_nt * (uv - n * dt) - n * sqrt(discriminant);
				return true;
			}
			return false;
		}
		inline float Schlick(float cosine, float ref_idx)
		{
			float r0 = (1 - ref_idx) / (1 + ref_idx);
			r0 = r0 * r0;
			return r0 + (1 - r0) * powf(1 - cosine, 5);
		}
		float BlueNoise(int x, int y, int frame)
		{
			int ix = (x + frame * 17) & (BN_SIZE - 1);
			int iy = (y + frame * 31) & (BN_SIZE - 1);
			return blueNoise[ix + iy * BN_SIZE] * (1.0f / 255.0f);
		}
		bool rebuildSphereBVH = false;
		float lastFrameTime = 0.0f;
		float avgFrameTimeMs = 0.0f;
		float fps = 0.0f;
		float rps = 0.0f;
		void Init();
		float3 Trace(Ray& ray, int = 0, int = 0, int = 0);
		void Tick(float deltaTime);
		void UIStats();
		void UI();
		void LightUI() const;
		static bool MaterialUI(const char* label, Material& material);
		void Shutdown() { /* nothing here for now */ }
		void MouseUp(int button) { button = 0; }
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
		void KeyDown(int key) { key = 0; }
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
		// Precomputed once — avoids normalize() sqrtf on every sphere pixel hit.
		float3 fastSphereLightDir = normalize(float3(0.5f, 0.8f, 0.3f));
		static constexpr float fastSphereAmbient = 0.12f;
		// Ray direction table — cached primary ray directions for stationary camera.
		float3* rayDirTable = nullptr;
		bool    rayTableDirty = true;
		uint32_t sampleCount = 0;
		mat4 lastViewMatrix;
		void InitAccumulator();
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
	};
} // namespace Tmpl8