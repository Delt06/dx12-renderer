#pragma once

#include <vector>

#include <DX12Library/Camera.h>
#include <Framework/GameObject.h>
#include <Framework/Light.h>
#include <Framework/BoundingSphere.h>
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
