#pragma once
#include <DirectXMath.h>

struct DirectionalLight
{
	DirectionalLight()
		: DirectionWs(1.0f, 0.0f, 1.0f, 0.0f)
		  , DirectionVs(0.0f, 0.0f, 1.0f, 0.0f)
		  , Color(1.0f, 1.0f, 1.0f, 1.0f)
	{
	}

	DirectX::XMFLOAT4 DirectionWs; // Light direction in world space.
	//----------------------------------- (16 byte boundary)
	DirectX::XMFLOAT4 DirectionVs; // Light direction in view space.
	//----------------------------------- (16 byte boundary)
	DirectX::XMFLOAT4 Color;
	//----------------------------------- (16 byte boundary)
	// Total:                              16 * 3 = 48 bytes 
};
