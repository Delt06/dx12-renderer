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

Microsoft::WRL::ComPtr<ID3D12ShaderReflection> ShaderUtils::Reflect(const Microsoft::WRL::ComPtr<ID3DBlob>& shaderSource)
{
	Microsoft::WRL::ComPtr<ID3D12ShaderReflection> reflection;
	ThrowIfFailed(D3DReflect(shaderSource->GetBufferPointer(), shaderSource->GetBufferSize(), IID_PPV_ARGS(&reflection)));
	return reflection;
}

std::vector<ShaderUtils::ConstantBufferMetadata> ShaderUtils::GetConstantBuffers(const Microsoft::WRL::ComPtr<ID3D12ShaderReflection>& shaderReflection)
{
	// https://github.com/planetpratik/DirectXTutorials/blob/master/SimpleShader.cpp
	D3D12_SHADER_DESC shaderDesc;
	ThrowIfFailed(shaderReflection->GetDesc(&shaderDesc));

	std::vector<ShaderUtils::ConstantBufferMetadata> result;
	result.reserve(shaderDesc.ConstantBuffers);

	for (UINT cbufferIndex = 0; cbufferIndex < shaderDesc.ConstantBuffers; ++cbufferIndex)
	{
		const auto constantBuffer = shaderReflection->GetConstantBufferByIndex(cbufferIndex);
		D3D12_SHADER_BUFFER_DESC cbufferDesc;
		ThrowIfFailed(constantBuffer->GetDesc(&cbufferDesc));

		D3D12_SHADER_INPUT_BIND_DESC inputBindDesc;
		ThrowIfFailed(shaderReflection->GetResourceBindingDescByName(cbufferDesc.Name, &inputBindDesc));

		ConstantBufferMetadata constantBufferMetadata;
		constantBufferMetadata.Name = cbufferDesc.Name;
		constantBufferMetadata.RegisterIndex = inputBindDesc.BindPoint;
		constantBufferMetadata.Space = inputBindDesc.Space;
		constantBufferMetadata.Variables.reserve(cbufferDesc.Variables);
		constantBufferMetadata.Size = 0;

		for (UINT variableIndex = 0; variableIndex < cbufferDesc.Variables; ++variableIndex)
		{
			auto variable = constantBuffer->GetVariableByIndex(variableIndex);
			D3D12_SHADER_VARIABLE_DESC variableDesc;
			ThrowIfFailed(variable->GetDesc(&variableDesc));

			ConstantBufferMetadata::Variable variableMetadata;
			variableMetadata.Name = variableDesc.Name;
			variableMetadata.Offset = variableDesc.StartOffset;
			variableMetadata.Size = variableDesc.Size;

			constantBufferMetadata.Size = max(constantBufferMetadata.Size, variableMetadata.Offset + variableMetadata.Size);
			constantBufferMetadata.Variables.push_back(variableMetadata);
		}

		result.push_back(constantBufferMetadata);
	}

	return result;
}

std::vector<ShaderUtils::ShaderResourceViewMetadata> ShaderUtils::GetShaderResourceViews(const Microsoft::WRL::ComPtr<ID3D12ShaderReflection>& shaderReflection)
{
	// https://github.com/planetpratik/DirectXTutorials/blob/master/SimpleShader.cpp
	D3D12_SHADER_DESC shaderDesc;
	ThrowIfFailed(shaderReflection->GetDesc(&shaderDesc));

	std::vector<ShaderUtils::ShaderResourceViewMetadata> result;
	result.reserve(shaderDesc.BoundResources);

	for (UINT resourceIndex = 0; resourceIndex < shaderDesc.BoundResources; ++resourceIndex)
	{
		D3D12_SHADER_INPUT_BIND_DESC inputBindDesc;
		ThrowIfFailed(shaderReflection->GetResourceBindingDesc(resourceIndex, &inputBindDesc));

		switch (inputBindDesc.Type)
		{
		case D3D_SIT_TEXTURE:
		{
			ShaderResourceViewMetadata resourceMetadata;
			resourceMetadata.Name = inputBindDesc.Name;
			resourceMetadata.RegisterIndex = inputBindDesc.BindPoint;
			resourceMetadata.Space = inputBindDesc.Space;
			result.push_back(resourceMetadata);
			break;
		}
		}
	}

	return result;
}
