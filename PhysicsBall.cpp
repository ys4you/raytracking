#include "template.h"
#include "PhysicsBall.h"

using namespace Tmpl8;

// ─── Static data ─────────────────────────────────────────────────────────────

const float3 PhysicsBall::GRAVITY = float3(0, -0.5f, 0);

static constexpr float S = 0.70710678f; // 1/sqrt(2)
static constexpr float T = 0.57735027f; // 1/sqrt(3)

const float3 PhysicsBall::s_probeDirections[PROBE_COUNT] = {
	// 6 cardinal axes
	float3(1, 0, 0), float3(-1, 0, 0),
	float3(0, 1, 0), float3(0, -1, 0),
	float3(0, 0, 1), float3(0, 0, -1),
	// 12 edge midpoints
	float3(S, S, 0), float3(S, -S, 0), float3(-S, S, 0), float3(-S, -S, 0),
	float3(S, 0, S), float3(S, 0, -S), float3(-S, 0, S), float3(-S, 0, -S),
	float3(0, S, S), float3(0, S, -S), float3(0, -S, S), float3(0, -S, -S),
	// 8 corner diagonals
	float3(T, T, T), float3(T, T, -T), float3(T, -T, T), float3(T, -T, -T),
	float3(-T, T, T), float3(-T, T, -T), float3(-T, -T, T), float3(-T, -T, -T),
};

// ─── Construction ────────────────────────────────────────────────────────────

PhysicsBall::PhysicsBall(float3 pos, float r, float m)
	: position(pos), radius(r), mass(m)
{
	rotationMat = mat4::Identity();
}

// ─── Probe terrain contact ───────────────────────────────────────────────────
// Only reacts to VOXEL hits (ray.voxel > 0).  Sphere hits (axis == 3) are
// ignored — sphere-sphere is handled separately by ResolvePair.

float PhysicsBall::ProbeTerrainContact(Scene& scene, float3& outNormal) const
{
	outNormal = float3(0);
	float maxPenetration = 0.0f;
	const float probeLen = radius + groundSnapDist;

	for (int i = 0; i < PROBE_COUNT; i++)
	{
		const float3 dir = s_probeDirections[i];
		Ray probe(position, dir, probeLen);
		scene.FindNearest(probe);

		// Only voxel hits count — ignore spheres and misses
		if (probe.voxel == 0) continue;
		if (probe.t >= radius) continue;

		float penetration = radius - probe.t;
		if (penetration > maxPenetration)
			maxPenetration = penetration;

		// Face normal from DDA axis
		float3 hitNormal;
		if (probe.axis == 0)      hitNormal = float3(probe.D.x < 0 ? 1.f : -1.f, 0, 0);
		else if (probe.axis == 1) hitNormal = float3(0, probe.D.y < 0 ? 1.f : -1.f, 0);
		else if (probe.axis == 2) hitNormal = float3(0, 0, probe.D.z < 0 ? 1.f : -1.f);
		else                        continue; // shouldn't happen, but safety

		outNormal += hitNormal * penetration;
	}

	float len = length(outNormal);
	if (len > 1e-6f)
		outNormal *= (1.0f / len);
	else
		outNormal = float3(0, 1, 0);

	return maxPenetration;
}

// ─── CCD sweep test ──────────────────────────────────────────────────────────
// Only blocks on VOXEL hits.  Sphere hits are ignored.

float PhysicsBall::SweepTest(Scene& scene, float dt) const
{
	float speed = length(velocity);
	if (speed < 1e-6f) return 0.0f;

	float travelDist = speed * dt;
	float3 dir = velocity * (1.0f / speed);

	Ray ccd(position, dir, travelDist + radius);
	scene.FindNearest(ccd);

	// Only voxel hits block movement
	if (ccd.voxel == 0) return travelDist;

	float safe = ccd.t - radius;
	return max(0.0f, safe);
}

// ─── Depenetration ───────────────────────────────────────────────────────────

void PhysicsBall::Depenetrate(float penetration, const float3& normal)
{
	if (penetration > 0.0f)
		position += normal * (penetration + 0.0001f);
}

// ─── Integrate rotation ──────────────────────────────────────────────────────

void PhysicsBall::IntegrateRotation(float dt)
{
	float speed = length(angularVel);
	if (speed < 1e-6f) return;

	float angle = speed * dt;
	float3 axis = angularVel * (1.0f / speed);

	float c = cosf(angle);
	float s = sinf(angle);
	float t = 1.0f - c;

	mat4 rot;
	rot.cell[0] = t * axis.x * axis.x + c;
	rot.cell[1] = t * axis.x * axis.y - s * axis.z;
	rot.cell[2] = t * axis.x * axis.z + s * axis.y;
	rot.cell[3] = 0;
	rot.cell[4] = t * axis.x * axis.y + s * axis.z;
	rot.cell[5] = t * axis.y * axis.y + c;
	rot.cell[6] = t * axis.y * axis.z - s * axis.x;
	rot.cell[7] = 0;
	rot.cell[8] = t * axis.x * axis.z - s * axis.y;
	rot.cell[9] = t * axis.y * axis.z + s * axis.x;
	rot.cell[10] = t * axis.z * axis.z + c;
	rot.cell[11] = 0;
	rot.cell[12] = 0; rot.cell[13] = 0; rot.cell[14] = 0; rot.cell[15] = 1;

	rotationMat = rot * rotationMat;
}

// ─── Main update ─────────────────────────────────────────────────────────────

void PhysicsBall::Update(float dt, Scene& scene)
{
	// 1. Gravity
	velocity += GRAVITY * dt;

	// 2. CCD: check for tunneling
	float safeDist = SweepTest(scene, dt);
	float speed = length(velocity);
	float fullDist = speed * dt;

	if (safeDist < fullDist && speed > 1e-6f)
	{
		float3 dir = velocity * (1.0f / speed);
		position += dir * safeDist;

		// Re-cast to get the normal at contact
		Ray ccd(position, dir, radius * 2.0f);
		scene.FindNearest(ccd);

		float3 hitN(0, 1, 0); // default up
		if (ccd.voxel > 0)
		{
			if (ccd.axis == 0)      hitN = float3(dir.x < 0 ? 1.f : -1.f, 0, 0);
			else if (ccd.axis == 1) hitN = float3(0, dir.y < 0 ? 1.f : -1.f, 0);
			else if (ccd.axis == 2) hitN = float3(0, 0, dir.z < 0 ? 1.f : -1.f);
		}

		float vn = dot(velocity, hitN);
		if (vn < 0)
			velocity -= (1.0f + restitution) * vn * hitN;
	}
	else
	{
		// 3. Normal integration (semi-implicit Euler)
		position += velocity * dt;
	}

	// 4. Probe terrain
	float3 contactNormal;
	float penetration = ProbeTerrainContact(scene, contactNormal);

	if (penetration > 0.0f)
	{
		Depenetrate(penetration, contactNormal);

		float vn = dot(velocity, contactNormal);

		if (vn < 0)
		{
			if (fabsf(vn) < 0.01f)
			{
				// Micro-bounce kill
				velocity -= vn * contactNormal;
				onGround = true;
			}
			else
			{
				// Bounce
				velocity -= (1.0f + restitution) * vn * contactNormal;
				onGround = false;
			}
		}
		else
		{
			onGround = true;
		}

		// 5. Rolling friction
		if (onGround)
		{
			float3 vTangent = velocity - dot(velocity, contactNormal) * contactNormal;
			float tangentSpeed = length(vTangent);

			if (tangentSpeed > 1e-6f)
			{
				float frictionAccel = rollingFriction * fabsf(dot(GRAVITY, contactNormal));
				float newSpeed = max(0.0f, tangentSpeed - frictionAccel * dt);
				velocity = (vTangent * (newSpeed / tangentSpeed))
					+ dot(velocity, contactNormal) * contactNormal;
			}

			// 6. Slope force
			float3 gravParallel = GRAVITY - dot(GRAVITY, contactNormal) * contactNormal;
			velocity += gravParallel * dt;
		}
	}
	else
	{
		onGround = false;
		velocity -= velocity * airDrag * dt;
	}

	// 7. World bounds clamp [radius, 1-radius]
	float lo = radius + 0.001f;
	float hi = 1.0f - radius - 0.001f;
	for (int a = 0; a < 3; a++)
	{
		float& p = (a == 0) ? position.x : (a == 1) ? position.y : position.z;
		float& v = (a == 0) ? velocity.x : (a == 1) ? velocity.y : velocity.z;
		if (p < lo) { p = lo; if (v < 0) v *= -restitution; }
		if (p > hi) { p = hi; if (v > 0) v *= -restitution; }
	}

	// 8. Angular velocity from rolling
	if (onGround)
	{
		float3 vTangent = velocity - dot(velocity, contactNormal) * contactNormal;
		angularVel = cross(contactNormal, vTangent) / radius;
	}

	IntegrateRotation(dt);

	// 9. Max speed clamp
	float maxSpeed = 0.5f * (1.0f / 256.0f) * 120.0f;
	if (length(velocity) > maxSpeed)
		velocity = normalize(velocity) * maxSpeed;
}

// ─── Sphere-sphere collision ─────────────────────────────────────────────────

void PhysicsBall::ResolvePair(PhysicsBall& a, PhysicsBall& b)
{
	float3 delta = b.position - a.position;
	float dist2 = dot(delta, delta);
	float minDist = a.radius + b.radius;

	if (dist2 >= minDist * minDist) return;
	if (dist2 < 1e-10f) return;

	float dist = sqrtf(dist2);
	float3 normal = delta * (1.0f / dist);

	float overlap = minDist - dist;
	float totalMass = a.mass + b.mass;
	a.position -= normal * (overlap * b.mass / totalMass * 0.5f);
	b.position += normal * (overlap * a.mass / totalMass * 0.5f);

	float relVn = dot(a.velocity - b.velocity, normal);
	if (relVn > 0) return;

	float e = min(a.restitution, b.restitution);
	float j = -(1.0f + e) * relVn / (1.0f / a.mass + 1.0f / b.mass);

	a.velocity += (j / a.mass) * normal;
	b.velocity -= (j / b.mass) * normal;
}