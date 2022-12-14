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
	explicit Material(const std::shared_ptr<Shader>& shader);
    explicit Material(const Material& materialPreset);

    Material& operator=(const Material& other) = delete;

	template<typename T>
	void SetAllVariables(const T& data)
	{
		SetAllVariables(sizeof(data), &data);
	}

	void SetAllVariables(size_t size, const void* data);

	template<typename T> void SetVariable(const std::string& name, const T& data, bool throwOnNotFound = true)
	{
		SetVariable(name, sizeof(data), &data, throwOnNotFound);
	}

	void SetVariable(const std::string& name, size_t size, const void* data, bool throwOnNotFound = true);
	void SetShaderResourceView(const std::string& name, const ShaderResourceView& shaderResourceView);

	void Bind(CommandList& commandList);
	void UploadUniforms(CommandList& commandList);
	void Unbind(CommandList& commandList);

	void BeginBatch(CommandList& commandList);
	void EndBatch(CommandList& commandList);

	static std::shared_ptr<Material> Create(const std::shared_ptr<Shader>& shader);
	static std::shared_ptr<Material> Create(const Material& materialPreset);

private:

	void UploadConstantBuffer(CommandList& commandList);
	void UploadShaderResourceViews(CommandList& commandList);

	std::shared_ptr<Shader> m_Shader;
	const ShaderUtils::ConstantBufferMetadata* m_Metadata;
	std::unique_ptr<uint8_t[]> m_ConstantBuffer = nullptr;
	size_t m_ConstantBufferSize;

	std::unordered_map <std::string, ShaderResourceView> m_ShaderResourceViews;
};
