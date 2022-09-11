#pragma once
#include <cstdint>

struct GraphicsSettings
{
	struct ShadowsSettings
	{
		uint32_t m_Resolution;
		float m_DepthBias;
		float m_NormalBias;
		float m_PoissonSpread;
	};

	bool m_VSync = false;

	ShadowsSettings m_DirectionalLightShadows{2048, 1.0f, 0.002f, 750.0f};
	// resolution is only a single cubemap side
	ShadowsSettings m_PointLightShadows{512, 0.5f, 0.001f, 250.0f};
};
