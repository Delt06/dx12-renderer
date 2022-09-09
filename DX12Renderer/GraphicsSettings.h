#pragma once
#include <cstdint>

struct GraphicsSettings
{
	bool m_VSync = false;
	uint32_t m_ShadowsResolution = 4096;
	uint32_t m_PointLightShadowsResolution = 1024;
	float m_ShadowsDepthBias = 1.0;
	float m_ShadowsNormalBias = 0.002f;
	float m_PoissonSpread = 750.0f;
};
