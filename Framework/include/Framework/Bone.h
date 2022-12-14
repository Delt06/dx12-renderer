#pragma once

#include <DirectXMath.h>
#include <string>

struct Bone
{
	std::string Name;
	DirectX::XMMATRIX Offset;
	DirectX::XMMATRIX LocalTransform;
	DirectX::XMMATRIX GlobalTransform;
	bool IsDirty;
};