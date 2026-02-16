#include "template.h"

Camera::Camera()
{

	// try to load a camera
	FILE* f = fopen( "camera.bin", "rb" );
	if (f)
	{
		fread( this, 1, sizeof( Camera ), f );
		fclose( f );
	}
	else
	{
		// setup a basic view frustum
		camPos = float3( 0, 0, -2 );
		camTarget = float3( 0, 0, -1 );
		topLeft = float3( -aspect, 1, 0 );
		topRight = float3( aspect, 1, 0 );
		bottomLeft = float3( -aspect, -1, 0 );
	}

	aperture = 0.1f;
	focusRange = float2(0.1f, .0f); // Focus on objects 4-6 units away
	blurFactor = 1.f;


	lastCamPos = camPos;
	lastCamTarget = camTarget;

}

Camera::~Camera()
{
	// save current camera
	FILE* f = fopen( "camera.bin", "wb" );
	fwrite( this, 1, sizeof( Camera ), f );
	fclose( f );
}

Ray Camera::GetPrimaryRay( const float x, const float y )
{
	// 1. Pixel on image plane
	float u = x / SCRWIDTH;
	float v = y / SCRHEIGHT;

	float3 P = topLeft +
		u * (topRight - topLeft) +
		v * (bottomLeft - topLeft);

	// 2. Pinhole ray direction
	float3 pinholeDir = normalize(P - camPos);

	// 3. Approximate distance along the ray (depth)
	float t = dot(pinholeDir, camAhead);

	// 4. Compute blur based on focus range
	float localBlur = 0.0f;
	if (t < focusRange.x)
		localBlur = (focusRange.x - t) / focusRange.x;
	else if (t > focusRange.y)
		localBlur = (t - focusRange.y) / focusRange.y;

	localBlur = clamp(localBlur, 0.0f, 1.0f);

	// Apply user-defined multiplier
	float finalBlur = localBlur * blurFactor;

	// 5. Sample point on lens
	float r = sqrt(RandomFloat());
	float theta = 2.0f * PI * RandomFloat();
	float dx = r * cos(theta);
	float dy = r * sin(theta);

	float3 lensOffset = camRight * dx * aperture * finalBlur +
		camUp * dy * aperture * finalBlur;

	// 6. Compute focus point
	float3 focusPoint = camPos + pinholeDir * t;

	float3 origin = camPos + lensOffset;
	float3 direction = normalize(focusPoint - origin);

	return Ray(origin, direction);


	// Note: no need to normalize primary rays in a pure voxel world
	// TODO: 
	// - if we have other primitives as well, we *do* need to normalize!
	// - there are far cooler camera models, e.g. try 'Panini projection'.
}

Ray Camera::GetPinholeRay(float x, float y)
{
	// 1. Pixel on image plane
	float u = x / SCRWIDTH;
	float v = y / SCRHEIGHT;

	float3 P = topLeft +
		u * (topRight - topLeft) +
		v * (bottomLeft - topLeft);

	// 2. Pinhole ray direction
	float3 pinholeDir = normalize(P - camPos);

	// 3. Calculate focus point at a fixed distance
	// For your scene, you probably want focusDistance around 5-10
	float focusDistance = (focusRange.x + focusRange.y) * 0.5f; // midpoint of focus range
	float3 focusPoint = camPos + pinholeDir * focusDistance;

	// 4. Sample point on lens (aperture)
	float r = sqrt(RandomFloat());
	float theta = 2.0f * PI * RandomFloat();
	float dx = r * cos(theta);
	float dy = r * sin(theta);

	float3 lensOffset = camRight * dx * aperture + camUp * dy * aperture;

	// 5. Ray from lens point through focus point
	float3 origin = camPos + lensOffset;
	float3 direction = normalize(focusPoint - origin);

	return Ray(origin, direction);
}

bool Camera::HandleInput( const float t )
{
	if (!WindowHasFocus()) return false;
	float speed = 0.0015f * t;
	float3 ahead = normalize( camTarget - camPos );
	float3 tmpUp( 0, 1, 0 );
	float3 right = normalize( cross( tmpUp, ahead ) );
	float3 up = normalize( cross( ahead, right ) );
	bool changed = false;

	if (IsKeyDown( GLFW_KEY_UP )) 
		camTarget -= speed * up, changed = true;
	if (IsKeyDown( GLFW_KEY_DOWN )) 

		camTarget += speed * up, changed = true;
	if (IsKeyDown( GLFW_KEY_LEFT ))
		camTarget -= speed * right, changed = true;

	if (IsKeyDown( GLFW_KEY_RIGHT )) 
		camTarget += speed * right, changed = true;

	ahead = normalize( camTarget - camPos );
	right = normalize( cross( tmpUp, ahead ) );
	up = normalize( cross( ahead, right ) );
	if (IsKeyDown( GLFW_KEY_A )) 
		camPos -= speed * right, changed = true;

	if (IsKeyDown( GLFW_KEY_D )) 
		camPos += speed * right, changed = true;

	if (GetAsyncKeyState( 'W' )) 
		camPos += speed * ahead, changed = true;

	if (IsKeyDown( GLFW_KEY_S )) 
		camPos -= speed * ahead, changed = true;

	if (IsKeyDown( GLFW_KEY_SPACE )) 
		camPos += speed * up, changed = true;

	if (IsKeyDown( GLFW_KEY_LEFT_CONTROL )) 
		camPos -= speed * up, changed = true;

	camTarget = camPos + ahead;
	ahead = normalize( camTarget - camPos );
	up = normalize( cross( ahead, right ) );
	right = normalize( cross( up, ahead ) );
	topLeft = camPos + 2.0f * ahead - aspect * right + up;
	topRight = camPos + 2.0f * ahead + aspect * right + up;
	bottomLeft = camPos + 2.0f * ahead - aspect * right - up;

	camRight = right;
	camUp = up;
	camAhead = ahead;

	if (!changed) 
		return false;
	return true;
}

bool Camera::CameraHasMoved()
{
	const float eps = 1e-6f;

	if (length(camPos - lastCamPos) > eps ||
		length(camTarget - lastCamTarget) > eps)
	{
		lastCamPos = camPos;
		lastCamTarget = camTarget;
		return true;
	}
	return false;
}
