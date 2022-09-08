#pragma once
#include <cstdint>

struct GraphicsSettings
{
	bool m_VSync = false;
	uint32_t m_ShadowsResolution = 4096;
	float m_ShadowsDepthBias = 1.0;
	float m_ShadowsNormalBias = 0.002f;
};
