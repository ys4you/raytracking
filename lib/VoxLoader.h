#pragma once

namespace Tmpl8 { class Scene; }

class VoxLoader
{
public:
    static bool Load(const char* path, Tmpl8::Scene& scene);
};