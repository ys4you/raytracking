#pragma once

// default screen resolution
#define SCRWIDTH	640
#define SCRHEIGHT	400
// #define FULLSCREEN
#define DOUBLESIZE

namespace Tmpl8 {

class Camera
{
public:
	Camera();
	~Camera();
	Ray GetPrimaryRay( const float x, const float y );
	Ray GetPinholeRay(float x, float y);
	bool HandleInput( const float t );
	bool CameraHasMoved();
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


};

}