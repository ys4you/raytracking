#pragma once
// ============================================================
// SceneManager.h
// ============================================================
#include "SceneDef.h"
#include <vector>

namespace Tmpl8 { class Scene; class Camera; }
struct SceneLights;
class Sky;

class SceneManager
{
public:
	int  AddScene(const SceneDef& def);
	void LoadScene(int id, Tmpl8::Scene& worldScene, Tmpl8::Camera& camera,
		Sky& sky, SceneLights& lights);

	SceneDef& Active();
	const SceneDef& Active() const;

	int  CurrentID()  const { return currentID; }
	int  SceneCount() const { return (int)scenes.size(); }
	bool HasActive()  const { return currentID >= 0; }

	// resetAccumulator passed through from Renderer so scene callbacks can use it
	void UI(Tmpl8::Scene& worldScene, Tmpl8::Camera& camera,
		Sky& sky, SceneLights& lights,
		std::function<void()> resetAccumulator = nullptr);

private:
	std::vector<SceneDef> scenes;
	int currentID = -1;
	SceneDef emptyScene;
};