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

CommonRootSignature::CommonRootSignature(const std::shared_ptr<Resource>& emptyResource)
	: m_EmptyShaderResourceView(emptyResource)
{
	auto& app = Application::Get();
	const auto device = app.GetDevice();

	D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData;
	featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
	if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
	{
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	}

	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	std::vector<CommonRootSignature::RootParameter> rootParameters(RootParameters::NumRootParameters);

	// constant buffers
	rootParameters[RootParameters::PipelineCBuffer].InitAsConstantBufferView(0u, 2u);
	rootParameters[RootParameters::MaterialCBuffer].InitAsConstantBufferView(0u, 0u);
	rootParameters[RootParameters::ModelCBuffer].InitAsConstantBufferView(0u, 1u);

	// descriptor tables
	DescriptorRange materialSrvRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, MATERIAL_SRVS_COUNT, 0u, MATERIAL_REGISTER_SPACE);
	rootParameters[RootParameters::MaterialSRVs].InitAsDescriptorTable(1, &materialSrvRange, D3D12_SHADER_VISIBILITY_ALL);

	DescriptorRange pipelineSrvRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, PIPELINE_SRVS_COUNT, 0u, PIPELINE_REGISTER_SPACE);
	rootParameters[RootParameters::PipelineSRVs].InitAsDescriptorTable(1, &pipelineSrvRange, D3D12_SHADER_VISIBILITY_PIXEL);


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

	this->SetRootSignatureDesc(rootSignatureDescription.Desc_1_1, featureData.HighestVersion);

	this->GetRootSignature()->SetName(L"Common Root Signature");
}

void CommonRootSignature::Bind(CommandList& commandList) const
{
	commandList.SetGraphicsRootSignature(*this);

	for (UINT i = 0; i < MATERIAL_SRVS_COUNT; ++i)
	{
		commandList.SetShaderResourceView(RootParameters::MaterialSRVs, i,
			*m_EmptyShaderResourceView.m_Resource,
			D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
			m_EmptyShaderResourceView.m_FirstSubresource, m_EmptyShaderResourceView.m_NumSubresources,
			m_EmptyShaderResourceView.m_Desc
		);
	}

	for (UINT i = 0; i < PIPELINE_SRVS_COUNT; ++i)
	{
		commandList.SetShaderResourceView(RootParameters::PipelineSRVs, i,
			*m_EmptyShaderResourceView.m_Resource,
			D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
			m_EmptyShaderResourceView.m_FirstSubresource, m_EmptyShaderResourceView.m_NumSubresources,
			m_EmptyShaderResourceView.m_Desc
		);
	}
}

void CommonRootSignature::SetPipelineConstantBuffer(CommandList& commandList, size_t size, const void* data) const
{
	commandList.SetGraphicsDynamicConstantBuffer(RootParameters::PipelineCBuffer, size, data);
}

void CommonRootSignature::SetMaterialConstantBuffer(CommandList& commandList, size_t size, const void* data) const
{
	commandList.SetGraphicsDynamicConstantBuffer(RootParameters::MaterialCBuffer, size, data);
}

void CommonRootSignature::SetModelConstantBuffer(CommandList& commandList, size_t size, const void* data) const
{
	commandList.SetGraphicsDynamicConstantBuffer(RootParameters::ModelCBuffer, size, data);
}

void CommonRootSignature::SetPipelineShaderResourceView(CommandList& commandList, UINT index, const ShaderResourceView& shaderResourceView) const
{
	Assert(index < PIPELINE_SRVS_COUNT, "Pipeline SRV index is out of bounds.");

	commandList.SetShaderResourceView(RootParameters::PipelineSRVs,
		index,
		*shaderResourceView.m_Resource,
		D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
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
		D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
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
