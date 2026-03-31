#pragma once
#include "PhysicsBall.h"

namespace Tmpl8
{

	class Scene;

	struct PhysicsWorld
	{
		static constexpr double PHYSICS_DT = 1.0 / 120.0;
		static constexpr int MAX_STEPS = 4;

		std::vector<PhysicsBall> balls;
		double accumulator = 0.0;

		/// <summary>Adds a simulated ball and returns its index.</summary>
		int AddBall(float3 position, float radius, float mass = 1.0f)
		{
			balls.emplace_back(position, radius, mass);
			return (int)balls.size() - 1;
		}

		/// <summary>Accumulates frame time and runs fixed simulation steps.</summary>
		void Update(float frameTime, Scene& scene)
		{
			accumulator += frameTime;
			if (accumulator > PHYSICS_DT * MAX_STEPS)
				accumulator = PHYSICS_DT * MAX_STEPS;

			while (accumulator >= PHYSICS_DT)
			{
				Step((float)PHYSICS_DT, scene);
				accumulator -= PHYSICS_DT;
			}
		}

		/// <summary>Runs one fixed physics step.</summary>
		void Step(float dt, Scene& scene)
		{
			for (auto& ball : balls)
				ball.Update(dt, scene);

			for (int i = 0; i < (int)balls.size(); i++)
				for (int j = i + 1; j < (int)balls.size(); j++)
					PhysicsBall::ResolvePair(balls[i], balls[j]);
		}
	};

}
