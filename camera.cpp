#include "template.h"

Camera::Camera()
{
	// Try loading a previously saved camera state from disk.
	FILE* f = fopen("camera.bin", "rb");
	if (f)
	{
		// Load entire camera struct (position, orientation, etc.)
		fread(this, 1, sizeof(Camera), f);
		fclose(f);
	}
	else
	{
		// Default camera setup if no saved camera exists.

		// Camera position
		camPos = float3(0, 0, -2);

		// Where the camera is looking
		camTarget = float3(0, 0, -1);

		// Image plane corners (used for pinhole camera rays)
		topLeft = float3(-aspect, 1, 0);
		topRight = float3(aspect, 1, 0);
		bottomLeft = float3(-aspect, -1, 0);
	}

	// Depth-of-field parameters
	aperture = 0.0f;              // Lens radius (0 = pinhole camera)
	focusRange = float2(0.0f, .0f); // Range where objects stay sharp
	blurFactor = .0f;             // Multiplier controlling blur strength

	// Panini projection parameters
	hfov = 120.0f;   // Horizontal field of view (degrees)
	panini_d = 0.0f; // Panini distance parameter (0 = pinhole)
	panini_s = 0.0f; // Vertical squeeze parameter

	// Used to detect if the camera moved
	lastCamPos = camPos;
	lastCamTarget = camTarget;
}

Camera::~Camera()
{
	// Save the camera state so the next run starts at the same location.
	FILE* f = fopen("camera.bin", "wb");
	fwrite(this, 1, sizeof(Camera), f);
	fclose(f);
}

float3 Camera::FisheyeBaseDir(float px, float py) const
{
	// Convert pixel coordinates to normalized screen coordinates [-1,1]
	float nx = (2.0f * px / SCRWIDTH - 1.0f);
	float ny = 1.0f - (2.0f * py / SCRHEIGHT);

	float aspectInv = (float)SCRHEIGHT / (float)SCRWIDTH;

	// Convert screen position into angular coordinates
	float hfovRad = hfov * PI / 180.0f;

	float phi = nx * hfovRad * 0.5f;                // horizontal angle
	float theta = ny * hfovRad * 0.5f * aspectInv;  // vertical angle

	// Convert spherical angles into a direction vector
	float3 dir;
	dir.x = cos(theta) * sin(phi);
	dir.y = sin(theta);
	dir.z = cos(theta) * cos(phi);

	return normalize(dir);
}

float3 Camera::PaniniBaseDir(float px, float py) const
{
	// Convert pixel to normalized screen coordinates
	float nx = (2.0f * px / SCRWIDTH - 1.0f);
	float ny = 1.0f - (2.0f * py / SCRHEIGHT);

	float aspectInv = (float)SCRHEIGHT / (float)SCRWIDTH;

	// Convert FOV to radians
	float hfovRad = hfov * PI / 180.0f;
	float halfFov = hfovRad * 0.5f;

	// Panini projection horizontal scaling
	float d = panini_d;

	// Maximum horizontal coordinate after projection
	float uMax = (d + 1.0f) * sin(halfFov) / (d + cos(halfFov));

	float u = nx * uMax;
	float v = ny * uMax * aspectInv;

	// Recover horizontal angle from Panini projection
	float dp1 = d + 1.0f;
	float phi =
		atan2(u, dp1) +
		asin(u * d / sqrt(dp1 * dp1 + u * u));

	float cosPhi = cos(phi);

	// Horizontal and vertical scaling
	float Sh = dp1 / (d + cosPhi);
	float Sv = (1.0f - panini_s) * Sh + panini_s / cosPhi;

	// Recover vertical angle
	float theta = atan(v / Sv);

	// Convert spherical coordinates to direction vector
	float3 dir;
	dir.x = cos(theta) * sin(phi);
	dir.y = sin(theta);
	dir.z = cos(theta) * cos(phi);

	return normalize(dir);
}

Ray Camera::GetPrimaryRay(const float x, const float y) const
{
	// Step 1: Generate direction in camera space
	float3 dirCam = (useFisheye)
		? FisheyeBaseDir(x + 0.5f, y + 0.5f)
		: PaniniBaseDir(x + 0.5f, y + 0.5f);

	// Transform from camera space to world space
	float3 dir =
		dirCam.x * camRight +
		dirCam.y * camUp +
		dirCam.z * camAhead;

	dir = normalize(dir);

	// Step 2: Estimate depth along camera forward axis
	float t = dot(dir, camAhead);

	// Step 3: Calculate depth-of-field blur amount
	float localBlur = 0.0f;

	if (t < focusRange.x)
		localBlur = (focusRange.x - t) / focusRange.x;
	else if (t > focusRange.y)
		localBlur = (t - focusRange.y) / focusRange.y;

	localBlur = clamp(localBlur, 0.0f, 1.0f);

	float finalBlur = localBlur * blurFactor;

	// Step 4: Sample random point on circular lens
	float r = sqrt(RandomFloat());
	float theta = 2.0f * PI * RandomFloat();

	float dx = r * cos(theta);
	float dy = r * sin(theta);

	float3 lensOffset =
		camRight * dx * aperture * finalBlur +
		camUp * dy * aperture * finalBlur;

	// Focus point on the view ray
	float3 focusPoint = camPos + dir * t;

	// Final ray origin and direction
	float3 origin = camPos + lensOffset;
	float3 direction = normalize(focusPoint - origin);

	return Ray(origin, direction);
}

Ray Camera::GetPinholeRay(float x, float y)
{
	// Convert pixel to normalized coordinates
	float u = x / SCRWIDTH;
	float v = y / SCRHEIGHT;

	// Compute pixel position on image plane
	float3 P = topLeft +
		u * (topRight - topLeft) +
		v * (bottomLeft - topLeft);

	// Direction from camera to pixel
	float3 pinholeDir = normalize(P - camPos);

	// Compute focus point at a fixed distance
	float focusDistance = (focusRange.x + focusRange.y) * 0.5f;
	float3 focusPoint = camPos + pinholeDir * focusDistance;

	// Random lens sample
	float r = sqrt(RandomFloat());
	float theta = 2.0f * PI * RandomFloat();

	float dx = r * cos(theta);
	float dy = r * sin(theta);

	float3 lensOffset = camRight * dx * aperture + camUp * dy * aperture;

	float3 origin = camPos + lensOffset;
	float3 direction = normalize(focusPoint - origin);

	return Ray(origin, direction);
}

bool Camera::HandleInput(const float t)
{
	if (!WindowHasFocus()) return false;

	float speed = 0.0015f * t;

	// Calculate camera orientation vectors
	float3 ahead = normalize(camTarget - camPos);
	float3 tmpUp(0, 1, 0);

	float3 right = normalize(cross(tmpUp, ahead));
	float3 up = normalize(cross(ahead, right));

	bool changed = false;

	// Rotate camera target with arrow keys
	if (IsKeyDown(GLFW_KEY_UP))
		camTarget -= speed * up, changed = true;

	if (IsKeyDown(GLFW_KEY_DOWN))
		camTarget += speed * up, changed = true;

	if (IsKeyDown(GLFW_KEY_LEFT))
		camTarget -= speed * right, changed = true;

	if (IsKeyDown(GLFW_KEY_RIGHT))
		camTarget += speed * right, changed = true;

	// Recalculate orientation vectors
	ahead = normalize(camTarget - camPos);
	right = normalize(cross(tmpUp, ahead));
	up = normalize(cross(ahead, right));

	// Camera movement
	if (IsKeyDown(GLFW_KEY_A))
		camPos -= speed * right, changed = true;

	if (IsKeyDown(GLFW_KEY_D))
		camPos += speed * right, changed = true;

	if (GetAsyncKeyState('W'))
		camPos += speed * ahead, changed = true;

	if (IsKeyDown(GLFW_KEY_S))
		camPos -= speed * ahead, changed = true;

	if (IsKeyDown(GLFW_KEY_SPACE))
		camPos += speed * up, changed = true;

	if (IsKeyDown(GLFW_KEY_LEFT_CONTROL))
		camPos -= speed * up, changed = true;

	// Keep camera target in front of the camera
	camTarget = camPos + ahead;

	// Rebuild camera basis vectors
	ahead = normalize(camTarget - camPos);
	up = normalize(cross(ahead, right));
	right = normalize(cross(up, ahead));

	// Recalculate image plane corners
	topLeft = camPos + 2.0f * ahead - aspect * right + up;
	topRight = camPos + 2.0f * ahead + aspect * right + up;
	bottomLeft = camPos + 2.0f * ahead - aspect * right - up;

	// Store basis vectors
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

	// Compare current and previous camera states
	if (length(camPos - lastCamPos) > eps ||
		length(camTarget - lastCamTarget) > eps)
	{
		lastCamPos = camPos;
		lastCamTarget = camTarget;
		return true;
	}
	return false;
}

bool Camera::WorldToScreen(const float3& P, float& outX, float& outY) const
{
	// Transform world position to camera space
	float3 dir = P - camPos;

	float cx = dot(dir, camRight);
	float cy = dot(dir, camUp);
	float cz = dot(dir, camAhead);

	// Point is behind the camera
	if (cz < 0.001f) return false;

	// Convert to spherical angles
	float len = sqrtf(cx * cx + cy * cy + cz * cz);

	float phi = atan2f(cx / len, cz / len);  // horizontal angle
	float theta = asinf(cy / len);           // vertical angle

	float hfovRad = hfov * PI / 180.0f;
	float halfFov = hfovRad * 0.5f;
	float aspectInv = (float)SCRHEIGHT / (float)SCRWIDTH;

	float nx, ny;

	if (useFisheye)
	{
		// Invert fisheye projection
		nx = phi / (halfFov);
		ny = theta / (halfFov * aspectInv);
	}
	else
	{
		// Invert Panini projection
		float d = panini_d;
		float s = panini_s;

		float dp1 = d + 1.0f;
		float cosPhi = cosf(phi);

		float u = dp1 * sinf(phi) / (d + cosPhi);

		float Sh = dp1 / (d + cosPhi);
		float Sv = (1.0f - s) * Sh + s / cosPhi;

		float v = tanf(theta) * Sv;

		float uMax = dp1 * sinf(halfFov) / (d + cosf(halfFov));

		nx = u / uMax;
		ny = v / (uMax * aspectInv);
	}

	// Convert normalized coordinates to screen pixels
	outX = (nx + 1.0f) * 0.5f * SCRWIDTH;
	outY = (1.0f - ny) * 0.5f * SCRHEIGHT;

	return (outX >= 0 && outX < SCRWIDTH - 1 &&
		outY >= 0 && outY < SCRHEIGHT - 1);
}

// reference: https://jacco.ompf2.com/2024/05/22/ray-tracing-with-voxels-in-c-series-part-5/
Frustum Camera::BuildFrustum()
{
	Frustum f;

	// Construct frustum planes from image plane edges
	f.plane[0] = cross(topLeft - bottomLeft, topLeft - camPos);   // left plane
	f.plane[1] = cross(topRight - camPos, topLeft - bottomLeft);  // right plane
	f.plane[2] = cross(topRight - topLeft, topLeft - camPos);     // top plane
	f.plane[3] = cross(bottomLeft - camPos, topRight - topLeft);  // bottom plane

	// Compute plane distances
	for (int i = 0; i < 4; i++)
	{
		f.plane[i].w = planeDist(f.plane[i], camPos);
	}

	return f;
}