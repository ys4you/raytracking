#pragma once

// default screen resolution
#define SCRWIDTH	800
#define SCRHEIGHT	600
//#define FULLSCREEN
#define DOUBLESIZE


typedef float4 Plane;
struct Frustum
{
	Plane plane[4];
};

inline float planeDist(Plane& plane, float3& pos)
{
	return dot(float3(plane), pos) - plane.w;
}

namespace Tmpl8 
{

class Camera
{
public:



	Camera();
	~Camera();
	float3 FisheyeBaseDir(float px, float py) const;
	float3 PaniniBaseDir(float px, float py) const;
	Ray GetPrimaryRay( const float x, const float y ) const;
	Ray GetPinholeRay(float x, float y);
	bool HandleInput( const float t );
	bool CameraHasMoved();

	bool WorldToScreen(const float3& P, float& outX, float& outY) const;

	float aspect = (float)SCRWIDTH / (float)SCRHEIGHT;
	float3 camPos, camTarget;
	float3 camRight;
	float3 camUp;
	float3 camAhead;

	float3 topLeft, topRight, bottomLeft;

	float3 lastCamPos;
	float3 lastCamTarget;

	float aperture = 0.05f;      // radius of lens
	float2 focusRange = float2(0.0f, 0.1f); // near and far distances in focus

	float blurFactor = 0.f;          // overall multiplier for blur, 0 = no blur, 1 = full blur

	//Panini params
	float hfov = 120.0f;   // horizontal FOV in degrees
	float panini_d = 1.0f; // 0 = pinhole, 1 = standard Panini
	float panini_s = 0.0f; // vertical squeeze (optional, start at 0)

	//fish eye
	bool useFisheye = false;

	Frustum BuildFrustum();

};

}