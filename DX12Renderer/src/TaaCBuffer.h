#pragma once

#include <DirectXMath.h>

struct TaaCBuffer
{
	DirectX::XMMATRIX PreviousModelViewProjectionMatrix;

	DirectX::XMFLOAT2 JitterOffset;
	float _Padding[2];
};

