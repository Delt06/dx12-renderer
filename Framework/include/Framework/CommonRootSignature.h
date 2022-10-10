#pragma once

#include <d3dx12.h>

#include <DX12Library/RootSignature.h>
#include <DX12Library/CommandList.h>
#include <DX12Library/Resource.h>

#include <vector>

#include "ShaderResourceView.h"

class CommonRootSignature final : public RootSignature
{
public:
	explicit CommonRootSignature(Microsoft::WRL::ComPtr<ID3D12Device2> device);

	inline void SetPipelineConstantBuffer(CommandList& commandList, size_t size, const void* data) const;

	template<typename T>
	inline void SetPipelineConstantBuffer(CommandList& commandList, const T& data) const
	{
		SetPipelineConstantBuffer(commandList, sizeof(T), &data);
	}


	inline void SetMaterialConstantBuffer(CommandList& commandList, size_t size, const void* data) const;

	inline void SetModelConstantBuffer(CommandList& commandList, size_t size, const void* data) const;
	template<typename T>
	inline void SetModelConstantBuffer(CommandList& commandList, const T& data) const
	{
		SetModelConstantBuffer(commandList, sizeof(T), &data);
	}

	void SetPipelineShaderResourceView(CommandList& commandList, UINT index, const ShaderResourceView& shaderResourceView) const;

	void SetMaterialShaderResourceView(CommandList& commandList, UINT index, const ShaderResourceView& shaderResourceView) const;

	static constexpr UINT MATERIAL_REGISTER_SPACE = 0u;
	static constexpr UINT MODEL_REGISTER_SPACE = 1u;
	static constexpr UINT PIPELINE_REGISTER_SPACE = 2u;

private:

	using RootParameter = CD3DX12_ROOT_PARAMETER1;
	using DescriptorRange = CD3DX12_DESCRIPTOR_RANGE1;
	using StaticSampler = CD3DX12_STATIC_SAMPLER_DESC;

	void CombineRootSignatureFlags(D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags, const std::vector<RootParameter>& rootParameters);
	bool CheckRootParametersVisiblity(const std::vector<RootParameter>& rootParameters, D3D12_SHADER_VISIBILITY param2);

	RootSignature m_RootSignature;
};