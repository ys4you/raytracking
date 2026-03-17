#pragma once

namespace Tmpl8
{

	class Scene;

	struct PhysicsBall
	{
		PhysicsBall() = default;
		PhysicsBall(float3 pos, float r, float m = 1.0f);

		void Update(float dt, Scene& scene);
		static void ResolvePair(PhysicsBall& a, PhysicsBall& b);

		// Tuning
		float restitution = 0.5f;
		float rollingFriction = 0.03f;
		float airDrag = 0.01f;
		float groundSnapDist = 0.002f;

		// State
		float3 position = float3(0);
		float3 velocity = float3(0);
		float3 angularVel = float3(0);
		float  radius = 0.02f;
		float  mass = 1.0f;
		bool   onGround = false;
		mat4   rotationMat;
		int    visualIndex = -1;

		// Constants — defined in .cpp (MSVC doesn't allow constexpr float3)
		static const float3 GRAVITY;
		static constexpr int PROBE_COUNT = 26;
		static const float3 s_probeDirections[PROBE_COUNT];

		// Helpers
		float ProbeTerrainContact(Scene& scene, float3& outNormal) const;
		float SweepTest(Scene& scene, float dt) const;
		void  Depenetrate(float penetration, const float3& normal);
		void  IntegrateRotation(float dt);
	};

} // namespace Tmpl8