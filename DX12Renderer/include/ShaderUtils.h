#pragma once

#include <string>
#include <d3d12.h>
#include <wrl.h>

namespace ShaderUtils
{
	Microsoft::WRL::ComPtr<ID3DBlob> LoadShaderFromFile(const std::wstring& fileName);
}