#include "ShaderUtils.h"
#include <d3dcompiler.h>
#include "Helpers.h"

Microsoft::WRL::ComPtr<ID3DBlob> ShaderUtils::LoadShaderFromFile(const std::wstring& fileName)
{
	Microsoft::WRL::ComPtr<ID3DBlob> result;
	const auto completePath = L"Shaders/" + fileName;
	ThrowIfFailed(D3DReadFileToBlob(completePath.c_str(), &result));
	return result;
}
