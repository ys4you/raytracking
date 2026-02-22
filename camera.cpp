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

	aperture = 0.0f;
	focusRange = float2(0.0f, .0f); // Focus on objects 4-6 units away
	blurFactor = .0f;

	//Panini params
	hfov = 120.0f;   // horizontal FOV in degrees
	panini_d = 0.0f; // 0 = pinhole, 1 = standard Panini
	panini_s = 0.0f; // vertical squeeze (optional, start at 0)


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

float3 Camera::FisheyeBaseDir(float px, float py) const
{
	// Normalize to [-1, 1]
	float nx = (2.0f * px / SCRWIDTH - 1.0f);
	float ny = 1.0f - (2.0f * py / SCRHEIGHT);
	float aspectInv = (float)SCRHEIGHT / (float)SCRWIDTH;

	// Map to angular space
	float hfovRad = hfov * PI / 180.0f;
	float phi = nx * hfovRad * 0.5f;              // azimuth
	float theta = ny * hfovRad * 0.5f * aspectInv;  // elevation

	float3 dir;
	dir.x = cos(theta) * sin(phi);
	dir.y = sin(theta);
	dir.z = cos(theta) * cos(phi);

	return normalize(dir);
}

float3 Camera::PaniniBaseDir(float px, float py) const
{
	//normalized
	float nx = (2.0f * px / SCRWIDTH - 1.0f);
	float ny = 1.0f - (2.0f * py / SCRHEIGHT);

	float aspectInv = (float)SCRHEIGHT / (float)SCRWIDTH;

	// FOV
	float hfovRad = hfov * PI / 180.0f;
	float halfFov = hfovRad * 0.5f;

	//Panini horizontal scaling
	float d = panini_d;
	float uMax = (d + 1.0f) * sin(halfFov) / (d + cos(halfFov));
	float u = nx * uMax;
	float v = ny * uMax * aspectInv;

	// recover horizontal angle
	float dp1 = d + 1.0f;
	float phi =
		atan2(u, dp1) +
		asin(u * d / sqrt(dp1 * dp1 + u * u));

	float cosPhi = cos(phi);
	float Sh = dp1 / (d + cosPhi);
	float Sv = (1.0f - panini_s) * Sh + panini_s / cosPhi;

	float theta = atan(v / Sv);

	// --- build direction in camera space --- AI helped
	float3 dir;
	dir.x = cos(theta) * sin(phi);
	dir.y = sin(theta);
	dir.z = cos(theta) * cos(phi);

	return normalize(dir);
}

Ray Camera::GetPrimaryRay( const float x, const float y ) const
{
	//.1 Panini 
	float3 dirCam = (useFisheye)
		? FisheyeBaseDir(x + 0.5f, y + 0.5f)
		: PaniniBaseDir(x + 0.5f, y + 0.5f);
	float3 dir =
		dirCam.x * camRight +
		dirCam.y * camUp +
		dirCam.z * camAhead;

	dir = normalize(dir);

	// 2. Approximate distance along the ray (depth)
	// depth along view direction
	float t = dot(dir, camAhead);

	// 3. Compute blur based on focus range
	float localBlur = 0.0f;
	if (t < focusRange.x)
		localBlur = (focusRange.x - t) / focusRange.x;
	else if (t > focusRange.y)
		localBlur = (t - focusRange.y) / focusRange.y;

	localBlur = clamp(localBlur, 0.0f, 1.0f);

	// Apply user-defined multiplier
	float finalBlur = localBlur * blurFactor;

	// 4. Sample point on lens
	float r = sqrt(RandomFloat());
	float theta = 2.0f * PI * RandomFloat();
	float dx = r * cos(theta);
	float dy = r * sin(theta);

	float3 lensOffset = camRight * dx * aperture * finalBlur +
						camUp * dy * aperture * finalBlur;

	float3 focusPoint = camPos + dir * t;


	float3 origin = camPos + lensOffset;
	float3 direction = normalize(focusPoint - origin);

	return Ray(origin, direction);
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
