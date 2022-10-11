#pragma once

#include <string>
#include <d3d12.h>
#include <wrl.h>
#include <d3d12shader.h>
#include <vector>
#include <memory>
#include <cstdint>

namespace ShaderUtils
{
	Microsoft::WRL::ComPtr<ID3DBlob> LoadShaderFromFile(const std::wstring& fileName);

	Microsoft::WRL::ComPtr<ID3D12ShaderReflection> Reflect(const Microsoft::WRL::ComPtr<ID3DBlob>& shaderSource);

	struct ConstantBufferMetadata
	{
		struct Variable
		{
			std::string Name;
			UINT Size;
			UINT Offset;
			std::shared_ptr<uint8_t[]> DefaultValue;
		};

		UINT RegisterIndex;
		UINT Space;
		UINT Size;
		std::string Name;
		std::vector<Variable> Variables;
	};


	std::vector<ConstantBufferMetadata> GetConstantBuffers(const Microsoft::WRL::ComPtr<ID3D12ShaderReflection>& shaderReflection);

	struct ShaderResourceViewMetadata
	{
		UINT RegisterIndex;
		UINT Space;
		std::string Name;
	};

	std::vector<ShaderResourceViewMetadata> GetShaderResourceViews(const Microsoft::WRL::ComPtr<ID3D12ShaderReflection>& shaderReflection);

	// TODO: GetSamplers
}