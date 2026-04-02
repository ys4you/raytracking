#pragma once
#include "SceneDef.h"
#include <vector>

namespace Tmpl8 { class Scene; class Camera; }
struct SceneLights;
class Sky;

class SceneManager
{
public:
	/// <summary>Adds a scene definition and returns its index.</summary>
	int  AddScene(const SceneDef& def);
	/// <summary>Loads a scene definition into runtime scene state.</summary>
	void LoadScene(const int id, Tmpl8::Scene& worldScene, Tmpl8::Camera& camera,
		Sky& sky, SceneLights& lights);

	/// <summary>Returns the currently active scene definition.</summary>
	SceneDef& Active();
	/// <summary>Returns the currently active scene definition as read-only.</summary>
	const SceneDef& Active() const;

	/// <summary>Returns the active scene index, or -1 when none is active.</summary>
	int  CurrentID()  const { return currentID; }
	/// <summary>Returns the number of registered scenes.</summary>
	int  SceneCount() const { return (int)scenes.size(); }
	/// <summary>Returns whether a scene is currently active.</summary>
	bool HasActive()  const { return currentID >= 0; }

	/// <summary>Draws scene selection and scene-level controls.</summary>
	void UI(Tmpl8::Scene& worldScene, Tmpl8::Camera& camera,
		Sky& sky, SceneLights& lights,
		std::function<void()> resetAccumulator = nullptr);

private:
	std::vector<SceneDef> scenes;
	int currentID = -1;
	SceneDef emptyScene;
};
