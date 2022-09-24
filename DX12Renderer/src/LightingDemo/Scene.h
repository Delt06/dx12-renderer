#pragma once

#include <vector>

#include "../GameObject.h"
#include "../Light.h"
#include "../Camera.h"
#include "../Geometry/BoundingSphere.h"
#include "ParticleSystem.h"

struct Scene
{
	Camera MainCamera;
	std::vector<GameObject> GameObjects;
	DirectionalLight MainDirectionalLight{};
	std::vector<PointLight> PointLights;
	std::vector<SpotLight> SpotLights;
	std::vector<ParticleSystem> ParticleSystems;

	[[nodiscard]] BoundingSphere ComputeBoundingSphere() const;
};
