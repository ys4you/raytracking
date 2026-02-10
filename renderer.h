#pragma once

class Light;
class PointLight;
class DirectionalLight;
class SpotLight;
class AreaLight;
class material;

namespace Tmpl8
{

class Renderer : public TheApp
{
public:
	//Claude
	inline float length2(const float3& v) {
		return v.x * v.x + v.y * v.y + v.z * v.z;
	}
	//Claude
	inline float3 RandomInUnitSphere() {
		float3 p;
		do { p = 2.0f * float3(RandomFloat(), RandomFloat(), RandomFloat()) - float3(1, 1, 1); } while (length2(p) >= 1.0f);
		return p;
	}

	inline float3 reflect(const float3& v, const float3& n) {
		return v - 2.0f * dot(v, n) * n;
	}

	inline bool Refract(const float3& v, const float3& n, float ni_over_nt, float3& refracted) {
		float3 uv = normalize(v);
		float dt = dot(uv, n);
		float discriminant = 1.0f - ni_over_nt * ni_over_nt * (1 - dt * dt);
		if (discriminant > 0) {
			refracted = ni_over_nt * (uv - n * dt) - n * sqrt(discriminant);
			return true;
		}
		return false;
	}

	inline float Schlick(float cosine, float ref_idx) {
		float r0 = (1 - ref_idx) / (1 + ref_idx);
		r0 = r0 * r0;
		return r0 + (1 - r0) * powf(1 - cosine, 5);
	}
	float avgFrameTimeMs = 16.67f; // smoothed frame time
	float fps = 60.f;
	float rps = 0.f; // million rays per second



	// game flow methods
	void Init();
	float3 Trace( Ray& ray, int = 0, int = 0, int = 0 );
	void Tick( float deltaTime );
	void UI();
	void LightUI() const;
	void MaterialUI(const char* label, Material& material);
	void Shutdown() { /* nothing here for now */ }
	// input handling
	void MouseUp( int button ) { button = 0; /* implement if you want to detect mouse button presses */ }
	void MouseDown( int button ) { button = 0; /* implement if you want to detect mouse button presses */ }
	void MouseMove( int x, int y )
	{
	#if defined(DOUBLESIZE) && !defined(FULLSCREEN)
		mousePos.x = x / 2, mousePos.y = y / 2;
	#else
		mousePos.x = x, mousePos.y = y;
	#endif
	}
	void MouseWheel( float y ) { y = 0; /* implement if you want to handle the mouse wheel */ }
	void KeyUp( int key ) { key = 0; /* implement if you want to handle keys */ }
	void KeyDown( int key ) { key = 0; /* implement if you want to handle keys */ }
	// data members
	int2 mousePos;
	float3* accumulator = nullptr;	// for episode 3
	float3* history;		// for episode 5
	Scene scene;
	Camera camera;


	// Lights
	std::vector<Light*> lights;
	PointLight* pointLight = nullptr;
	DirectionalLight* dirLight = nullptr;
	SpotLight* spotLight = nullptr;
	AreaLight* areaLight = nullptr;

	bool debugNormals = false;

	uint32_t sampleCount = 0;
	mat4 lastViewMatrix;

	void InitAccumulator();

	void ResetAccumulator();
};

} // namespace Tmpl8