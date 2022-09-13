#pragma once

#include <DirectXMath.h>
#include <string>

struct Bone
{
	std::string Name;
	DirectX::XMMATRIX Transform;
};