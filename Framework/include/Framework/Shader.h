#pragma once

#include <d3d12.h>
#include <d3dx12.h>

#include <wrl.h>
#include <DX12Library/CommandList.h>
#include <DX12Library/ShaderUtils.h>
#include "CommonRootSignature.h"

#include <string>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

#include "ShaderResourceView.h"
#include "PipelineStateBuilder.h"

class Shader
{
public:
	explicit Shader(
		const std::shared_ptr<CommonRootSignature>& rootSignature,
		const std::wstring& vertexShaderPath, const std::wstring& pixelShaderPath,
		const std::function<void(PipelineStateBuilder&)> buildPipelineState
	);

	Shader(const Shader& other) = delete;
	Shader& operator=(const Shader& other) = delete;
	Shader(Shader&& other) = delete;
	Shader& operator=(Shader&& other) = delete;

	void Bind(CommandList& commandList);

	template<typename T>
	void SetPipelineConstantBuffer(CommandList& commandList, const T& data)
	{
		m_RootSignature->SetPipelineConstantBuffer(commandList, data);
	}

	template<typename T>
	void SetModelConstantBuffer(CommandList& commandList, const T& data)
	{
		m_RootSignature->SetMaterialConstantBuffer(commandList, data);
	}

	void SetMaterialConstantBuffer(CommandList& commandList, size_t size, const void* data);

	void SetShaderResourceView(CommandList& commandList, const std::string& variableName, const ShaderResourceView& shaderResourceView);

	struct ShaderMetadata
	{
		using NameCacheMap = std::map<std::string, size_t>;

		std::vector<ShaderUtils::ConstantBufferMetadata> m_ConstantBuffers{};
		NameCacheMap m_ConstantBuffersNameCache{};

		std::vector<ShaderUtils::ShaderResourceViewMetadata> m_ShaderResourceViews{};
		NameCacheMap m_ShaderResourceViewsNameCache{};
	};

	const ShaderMetadata& GetVertexShaderMetadata() const { return m_VertexShaderMetadata; }
	const ShaderMetadata& GetPixelShaderMetadata() const { return m_PixelShaderMetadata; }

private:


	Microsoft::WRL::ComPtr<ID3D12PipelineState> GetPipelineState(const Microsoft::WRL::ComPtr<ID3D12Device2>& device, const RenderTargetFormats& formats);

	void CollectShaderMetadata(const Microsoft::WRL::ComPtr<ID3DBlob>& shader, ShaderMetadata* outMetadata);

	std::shared_ptr<CommonRootSignature> m_RootSignature;

	ShaderMetadata m_VertexShaderMetadata;
	ShaderMetadata m_PixelShaderMetadata;

	PipelineStateBuilder m_PipelineStateBuilder;
	std::unordered_map<RenderTargetFormats, Microsoft::WRL::ComPtr<ID3D12PipelineState>> m_PipelineStateObjects;
};