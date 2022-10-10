#pragma once

#include <cstdint>
#include <string>
#include <memory>
#include <unordered_map>

#include <DX12Library/CommandList.h>

#include <wrl.h>
#include <d3d12.h>

#include "Shader.h"
#include "ShaderResourceView.h"

class Material
{
public:
	template<typename T>
	void SetVariable(const std::string& name, const T& data)
	{
		SetVariable(name, sizeof(data), &data);
	}

	void SetVariable(const std::string& name, size_t size, const void* data);
	void SetShaderResourceView(const std::string& name, const ShaderResourceView& shaderResourceView);

	void Bind(CommandList& commandList);

	static std::shared_ptr<Material> Create(const std::shared_ptr<Shader>& shader);

private:

	explicit Material(const std::shared_ptr<Shader>& shader);

	void UploadConstantBuffer(CommandList& commandList);
	void UploadShaderResourceViews(CommandList& commandList);

	std::shared_ptr<Shader> m_Shader;
	const ShaderUtils::ConstantBufferMetadata* m_Metadata;
	std::unique_ptr<uint8_t[]> m_ConstantBuffer;
	size_t m_ConstantBufferSize;

	std::unordered_map <std::string, ShaderResourceView> m_ShaderResourceViews;
};