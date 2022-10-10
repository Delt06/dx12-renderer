#include "CommonRootSignature.h"
#include <DX12Library/Helpers.h>

namespace
{
	namespace RootParameters
	{
		enum
		{
			// sorted by the change frequency (high to low)
			ModelCBuffer,

			MaterialCBuffer,
			MaterialSRVs,

			PipelineCBuffer,
			PipelineSRVs,

			NumRootParameters,
		};
	}

	static constexpr UINT PIPELINE_SRVS_COUNT = 32;
	static constexpr UINT MATERIAL_SRVS_COUNT = 6;
}

CommonRootSignature::CommonRootSignature(Microsoft::WRL::ComPtr<ID3D12Device2> device)
{
	D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData;
	featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
	if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
	{
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	}

	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	std::vector<CommonRootSignature::RootParameter> rootParameters(RootParameters::NumRootParameters);

	// constant buffers
	{
		rootParameters[RootParameters::PipelineCBuffer].InitAsConstantBufferView(0u, 2u);
		rootParameters[RootParameters::MaterialCBuffer].InitAsConstantBufferView(0u, 0u);
		rootParameters[RootParameters::ModelCBuffer].InitAsConstantBufferView(0u, 1u);
	}


	// descriptor tables
	{
		{
			DescriptorRange srvRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, MATERIAL_SRVS_COUNT, 0u, 0u);
			rootParameters[RootParameters::MaterialSRVs].InitAsDescriptorTable(1, &srvRange, D3D12_SHADER_VISIBILITY_ALL);
		}

		{
			DescriptorRange srvRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, PIPELINE_SRVS_COUNT, 0u, 2u);
			rootParameters[RootParameters::PipelineSRVs].InitAsDescriptorTable(1, &srvRange, D3D12_SHADER_VISIBILITY_PIXEL);
		}
	}


	CombineRootSignatureFlags(rootSignatureFlags, rootParameters);

	const StaticSampler staticSamples[] =
	{
		StaticSampler(0, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT),
		StaticSampler(1, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR),
	};

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
	rootSignatureDescription.Init_1_1(
		static_cast<UINT>(rootParameters.size()), rootParameters.data(),
		static_cast<UINT>(_countof(staticSamples)), staticSamples,
		rootSignatureFlags);

	m_RootSignature.SetRootSignatureDesc(rootSignatureDescription.Desc_1_1, featureData.HighestVersion);
}

inline void CommonRootSignature::SetPipelineConstantBuffer(CommandList& commandList, size_t size, const void* data) const
{
	commandList.SetGraphicsDynamicConstantBuffer(RootParameters::PipelineCBuffer, size, data);
}

inline void CommonRootSignature::SetMaterialConstantBuffer(CommandList& commandList, size_t size, const void* data) const
{
	commandList.SetGraphicsDynamicConstantBuffer(RootParameters::MaterialCBuffer, size, data);
}

inline void CommonRootSignature::SetModelConstantBuffer(CommandList& commandList, size_t size, const void* data) const
{
	commandList.SetGraphicsDynamicConstantBuffer(RootParameters::ModelCBuffer, size, data);
}

void CommonRootSignature::SetPipelineShaderResourceView(CommandList& commandList, UINT index, const ShaderResourceView& shaderResourceView) const
{
	Assert(index < PIPELINE_SRVS_COUNT, "Pipeline SRV index is out of bounds.");

	commandList.SetShaderResourceView(RootParameters::PipelineSRVs,
		index,
		*shaderResourceView.m_Resource,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		shaderResourceView.m_FirstSubresource, shaderResourceView.m_NumSubresources,
		shaderResourceView.m_Desc
	);
}

void CommonRootSignature::SetMaterialShaderResourceView(CommandList& commandList, UINT index, const ShaderResourceView& shaderResourceView) const
{
	Assert(index < MATERIAL_SRVS_COUNT, "Material SRV index is out of bounds.");

	commandList.SetShaderResourceView(RootParameters::MaterialSRVs,
		index,
		*shaderResourceView.m_Resource,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
		shaderResourceView.m_FirstSubresource, shaderResourceView.m_NumSubresources,
		shaderResourceView.m_Desc
	);
}

void CommonRootSignature::CombineRootSignatureFlags(D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags, const std::vector<RootParameter>& rootParameters)
{
	if (!CheckRootParametersVisiblity(rootParameters, D3D12_SHADER_VISIBILITY_VERTEX))
	{
		rootSignatureFlags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS;
	}

	if (!CheckRootParametersVisiblity(rootParameters, D3D12_SHADER_VISIBILITY_GEOMETRY))
	{
		rootSignatureFlags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
	}

	if (!CheckRootParametersVisiblity(rootParameters, D3D12_SHADER_VISIBILITY_MESH))
	{
		rootSignatureFlags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS;
	}

	if (!CheckRootParametersVisiblity(rootParameters, D3D12_SHADER_VISIBILITY_DOMAIN))
	{
		rootSignatureFlags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;
	}

	if (!CheckRootParametersVisiblity(rootParameters, D3D12_SHADER_VISIBILITY_HULL))
	{
		rootSignatureFlags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;
	}

	if (!CheckRootParametersVisiblity(rootParameters, D3D12_SHADER_VISIBILITY_PIXEL))
	{
		rootSignatureFlags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;
	}
}

bool CommonRootSignature::CheckRootParametersVisiblity(const std::vector<RootParameter>& rootParameters, D3D12_SHADER_VISIBILITY shaderVisibility)
{
	for (const auto& rootParameter : rootParameters)
	{
		if (rootParameter.ShaderVisibility == D3D12_SHADER_VISIBILITY_ALL)
		{
			return true;
		}

		if (rootParameter.ShaderVisibility == shaderVisibility)
		{
			return true;
		}
	}

	return false;

}
