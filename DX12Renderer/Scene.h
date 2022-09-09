#pragma once

#include <vector>

#include "GameObject.h"
#include "Light.h"
#include "Camera.h"
#include "Geometry/BoundingSphere.h"

struct Scene
{
	Camera m_Camera;
	std::vector<GameObject> m_GameObjects;
	DirectionalLight m_DirectionalLight{};
	std::vector<PointLight> m_PointLights;

	[[nodiscard]] BoundingSphere ComputeBoundingSphere() const;
};
