#include "template.h"

Camera::Camera()
{
	FILE* f = fopen("camera.bin", "rb");
	if (f)
	{
		fread(this, 1, sizeof(Camera), f);
		fclose(f);
	}
	else
	{

		camPos = float3(0, 0, -2);

		camTarget = float3(0, 0, -1);

		topLeft = float3(-aspect, 1, 0);
		topRight = float3(aspect, 1, 0);
		bottomLeft = float3(-aspect, -1, 0);
	}

	aperture = 0.0f;
	focusRange = float2(0.0f, .0f);
	blurFactor = .0f;

	hfov = 120.0f;
	panini_d = 0.0f;
	panini_s = 0.0f;

	lastCamPos = camPos;
	lastCamTarget = camTarget;
}

Camera::~Camera()
{
	FILE* f = fopen("camera.bin", "wb");
	fwrite(this, 1, sizeof(Camera), f);
	fclose(f);
}

float3 Camera::FisheyeBaseDir(float px, float py) const
{
	const float nx = (2.0f * px / SCRWIDTH - 1.0f);
	const float ny = 1.0f - (2.0f * py / SCRHEIGHT);

	const float aspectInv = (float)SCRHEIGHT / (float)SCRWIDTH;

	const float hfovRad = hfov * PI / 180.0f;

	const float phi = nx * hfovRad * 0.5f;
	const float theta = ny * hfovRad * 0.5f * aspectInv;

	float3 dir;
	dir.x = cos(theta) * sin(phi);
	dir.y = sin(theta);
	dir.z = cos(theta) * cos(phi);

	return normalize(dir);
}

float3 Camera::PaniniBaseDir(float px, float py) const
{
	const float nx = (2.0f * px / SCRWIDTH - 1.0f);
	const float ny = 1.0f - (2.0f * py / SCRHEIGHT);

	const float aspectInv = (float)SCRHEIGHT / (float)SCRWIDTH;

	const float hfovRad = hfov * PI / 180.0f;
	const float halfFov = hfovRad * 0.5f;

	const float d = panini_d;

	float uMax = (d + 1.0f) * sin(halfFov) / (d + cos(halfFov));

	float u = nx * uMax;
	float v = ny * uMax * aspectInv;

	const float dp1 = d + 1.0f;
	const float phi =
		atan2(u, dp1) +
		asin(u * d / sqrt(dp1 * dp1 + u * u));

	const float cosPhi = cos(phi);

	float Sh = dp1 / (d + cosPhi);
	float Sv = (1.0f - panini_s) * Sh + panini_s / cosPhi;

	const float theta = atan(v / Sv);

	float3 dir;
	dir.x = cos(theta) * sin(phi);
	dir.y = sin(theta);
	dir.z = cos(theta) * cos(phi);

	return normalize(dir);
}

Ray Camera::GetPrimaryRay(const float x, const float y) const
{
	const float3 dirCam = (useFisheye)
		? FisheyeBaseDir(x + 0.5f, y + 0.5f)
		: PaniniBaseDir(x + 0.5f, y + 0.5f);

	float3 dir =
		dirCam.x * camRight +
		dirCam.y * camUp +
		dirCam.z * camAhead;

	dir = normalize(dir);

	const float t = dot(dir, camAhead);

	float localBlur = 0.0f;

	if (t < focusRange.x)
		localBlur = (focusRange.x - t) / focusRange.x;
	else if (t > focusRange.y)
		localBlur = (t - focusRange.y) / focusRange.y;

	localBlur = clamp(localBlur, 0.0f, 1.0f);

	const float finalBlur = localBlur * blurFactor;

	const float r = sqrt(RandomFloat());
	const float theta = 2.0f * PI * RandomFloat();

	const float dx = r * cos(theta);
	const float dy = r * sin(theta);

	float3 lensOffset =
		camRight * dx * aperture * finalBlur +
		camUp * dy * aperture * finalBlur;

	const float3 focusPoint = camPos + dir * t;

	float3 origin = camPos + lensOffset;
	float3 direction = normalize(focusPoint - origin);

	return Ray(origin, direction);
}

Ray Camera::GetPinholeRay(const float x, const float y) const
{
	const float u = x / SCRWIDTH;
	const float v = y / SCRHEIGHT;

	float3 P = topLeft +
		u * (topRight - topLeft) +
		v * (bottomLeft - topLeft);

	const float3 pinholeDir = normalize(P - camPos);

	const float focusDistance = (focusRange.x + focusRange.y) * 0.5f;
	const float3 focusPoint = camPos + pinholeDir * focusDistance;

	const float r = sqrt(RandomFloat());
	const float theta = 2.0f * PI * RandomFloat();

	const float dx = r * cos(theta);
	const float dy = r * sin(theta);

	float3 lensOffset = camRight * dx * aperture + camUp * dy * aperture;

	float3 origin = camPos + lensOffset;
	float3 direction = normalize(focusPoint - origin);

	return Ray(origin, direction);
}

bool Camera::HandleInput(const float t)
{
	if (!WindowHasFocus()) return false;

	const float speed = 0.0015f * t;

	float3 ahead = normalize(camTarget - camPos);
	const float3 tmpUp(0, 1, 0);

	float3 right = normalize(cross(tmpUp, ahead));
	float3 up = normalize(cross(ahead, right));

	bool changed = false;

	if (IsKeyDown(GLFW_KEY_UP))
		camTarget -= speed * up, changed = true;

	if (IsKeyDown(GLFW_KEY_DOWN))
		camTarget += speed * up, changed = true;

	if (IsKeyDown(GLFW_KEY_LEFT))
		camTarget -= speed * right, changed = true;

	if (IsKeyDown(GLFW_KEY_RIGHT))
		camTarget += speed * right, changed = true;

	ahead = normalize(camTarget - camPos);
	right = normalize(cross(tmpUp, ahead));
	up = normalize(cross(ahead, right));

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

	camTarget = camPos + ahead;

	ahead = normalize(camTarget - camPos);
	up = normalize(cross(ahead, right));
	right = normalize(cross(up, ahead));

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

bool Camera::WorldToScreen(const float3& P, float& outX, float& outY) const
{
	const float3 dir = P - camPos;

	const float cx = dot(dir, camRight);
	const float cy = dot(dir, camUp);
	const float cz = dot(dir, camAhead);

	if (cz < 0.001f) return false;

	const float len = sqrtf(cx * cx + cy * cy + cz * cz);

	const float phi = atan2f(cx / len, cz / len);
	const float theta = asinf(cy / len);

	const float hfovRad = hfov * PI / 180.0f;
	const float halfFov = hfovRad * 0.5f;
	const float aspectInv = (float)SCRHEIGHT / (float)SCRWIDTH;

	float nx, ny;

	if (useFisheye)
	{
		nx = phi / (halfFov);
		ny = theta / (halfFov * aspectInv);
	}
	else
	{
		const float d = panini_d;
		const float s = panini_s;

		const float dp1 = d + 1.0f;
		const float cosPhi = cosf(phi);

		const float u = dp1 * sinf(phi) / (d + cosPhi);

		const float Sh = dp1 / (d + cosPhi);
		const float Sv = (1.0f - s) * Sh + s / cosPhi;

		const float v = tanf(theta) * Sv;

		const float uMax = dp1 * sinf(halfFov) / (d + cosf(halfFov));

		nx = u / uMax;
		ny = v / (uMax * aspectInv);
	}

	outX = (nx + 1.0f) * 0.5f * SCRWIDTH;
	outY = (1.0f - ny) * 0.5f * SCRHEIGHT;

	return (outX >= 0 && outX < SCRWIDTH - 1 &&
		outY >= 0 && outY < SCRHEIGHT - 1);
}

Frustum Camera::BuildFrustum() const
{
	Frustum f;

	f.plane[0] = cross(topLeft - bottomLeft, topLeft - camPos);
	f.plane[1] = cross(topRight - camPos, topLeft - bottomLeft);
	f.plane[2] = cross(topRight - topLeft, topLeft - camPos);
	f.plane[3] = cross(bottomLeft - camPos, topRight - topLeft);

	for (int i = 0; i < 4; i++)
	{
		f.plane[i].w = planeDist(f.plane[i], camPos);
	}

	return f;
}
