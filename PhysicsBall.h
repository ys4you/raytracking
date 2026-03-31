#pragma once

namespace Tmpl8
{

	class Scene;

	struct PhysicsBall
	{
		/// <summary>Creates a default physics ball.</summary>
		PhysicsBall() = default;
		/// <summary>Creates a physics ball with initial state.</summary>
		PhysicsBall(float3 pos, float r, float m = 1.0f);

		/// <summary>Advances ball simulation by one timestep.</summary>
		void Update(float dt, Scene& scene);
		/// <summary>Resolves collision response between two balls.</summary>
		static void ResolvePair(PhysicsBall& a, PhysicsBall& b);

		float restitution = 0.5f;
		float rollingFriction = 0.03f;
		float airDrag = 0.01f;
		float groundSnapDist = 0.002f;

		float3 position = float3(0);
		float3 velocity = float3(0);
		float3 angularVel = float3(0);
		float  radius = 0.02f;
		float  mass = 1.0f;
		bool   onGround = false;
		mat4   rotationMat;
		int    visualIndex = -1;

		static const float3 GRAVITY;
		static constexpr int PROBE_COUNT = 26;
		static const float3 s_probeDirections[PROBE_COUNT];

		/// <summary>Samples terrain contact depth and normal.</summary>
		float ProbeTerrainContact(Scene& scene, float3& outNormal) const;
		/// <summary>Performs swept collision detection for ball movement.</summary>
		float SweepTest(Scene& scene, float dt) const;
		/// <summary>Pushes the ball out of penetration along a normal.</summary>
		void  Depenetrate(float penetration, const float3& normal);
		/// <summary>Updates orientation from angular velocity.</summary>
		void  IntegrateRotation(float dt);
	};

}
