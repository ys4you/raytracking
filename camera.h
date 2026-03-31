#pragma once

#define SCRWIDTH	/*1920*/800
#define SCRHEIGHT	/*1080*/600


typedef float4 Plane;
struct Frustum
{
	Plane plane[4];
};

inline float planeDist(const Plane& plane, const float3& pos)
{
	return dot(float3(plane), pos) - plane.w;
}

namespace Tmpl8
{

class Camera
{
public:
	/// <summary>Initializes camera defaults.</summary>
	Camera();
	/// <summary>Releases camera resources.</summary>
	~Camera();
	/// <summary>Builds a fisheye base direction for a pixel location.</summary>
	float3 FisheyeBaseDir(float px, float py) const;
	/// <summary>Builds a Panini-projected base direction for a pixel location.</summary>
	float3 PaniniBaseDir(float px, float py) const;
	/// <summary>Returns the primary camera ray for normalized screen coordinates.</summary>
	Ray GetPrimaryRay( const float x, const float y ) const;
	/// <summary>Returns a pinhole camera ray for normalized screen coordinates.</summary>
	Ray GetPinholeRay(float x, float y);
	/// <summary>Processes camera input and reports whether state changed.</summary>
	bool HandleInput( const float t );
	/// <summary>Returns whether the camera moved since the previous frame.</summary>
	bool CameraHasMoved();

	/// <summary>Projects a world-space position to screen coordinates.</summary>
	bool WorldToScreen(const float3& P, float& outX, float& outY) const;

	float aspect = static_cast<float>(SCRWIDTH) / static_cast<float>(SCRHEIGHT);
	float3 camPos, camTarget;
	float3 camRight;
	float3 camUp;
	float3 camAhead;

	float3 topLeft, topRight, bottomLeft;

	float3 lastCamPos;
	float3 lastCamTarget;

	float aperture = 0.05f;
	float2 focusRange = float2(0.0f, 0.1f);

	float blurFactor = 0.f;

	float hfov = 120.0f;
	float panini_d = 1.0f;
	float panini_s = 0.0f;

	bool useFisheye = false;

	/// <summary>Builds frustum planes for the current camera transform.</summary>
	Frustum BuildFrustum();

};

}
