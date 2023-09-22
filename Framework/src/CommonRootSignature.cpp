#include "CommonRootSignature.h"
#include <DX12Library/Helpers.h>
#include <DX12Library/StructuredBuffer.h>

namespace
{
    constexpr UINT PIPELINE_SRVS_COUNT = 32;
    constexpr UINT MATERIAL_SRVS_COUNT = 6;
    constexpr UINT UAVS_COUNT = 6;
}

CommonRootSignature::CommonRootSignature(const std::shared_ptr<Resource>& emptyResource)
    : m_EmptySRV(emptyResource)
    , m_EmptyUAV(std::make_shared<StructuredBuffer>(
        CD3DX12_RESOURCE_DESC::Buffer(sizeof(uint32_t), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
        1, 1, L"Empty Buffer"
    ))
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
    std::vector<RootParameter> rootParameters(RootParameters::NumRootParameters);

    // constant buffers
    rootParameters[RootParameters::Constants].InitAsConstants(4u, 0u, CONSTANTS_REGISTER_SPACE);
    rootParameters[RootParameters::PipelineCBuffer].InitAsConstantBufferView(0u, PIPELINE_REGISTER_SPACE);
    rootParameters[RootParameters::MaterialCBuffer].InitAsConstantBufferView(0u, MATERIAL_REGISTER_SPACE);
    rootParameters[RootParameters::ModelCBuffer].InitAsConstantBufferView(0u, MODEL_REGISTER_SPACE);

    // descriptor tables
    DescriptorRange materialSrvRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, MATERIAL_SRVS_COUNT, 0u, MATERIAL_REGISTER_SPACE);
    rootParameters[RootParameters::MaterialSRVs].InitAsDescriptorTable(1, &materialSrvRange, D3D12_SHADER_VISIBILITY_ALL);

    DescriptorRange pipelineSrvRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, PIPELINE_SRVS_COUNT, 0u, PIPELINE_REGISTER_SPACE);
    rootParameters[RootParameters::PipelineSRVs].InitAsDescriptorTable(1, &pipelineSrvRange, D3D12_SHADER_VISIBILITY_ALL);

    DescriptorRange uavsRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, UAVS_COUNT, 0u, 0u, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
    rootParameters[RootParameters::UAVs].InitAsDescriptorTable(1, &uavsRange, D3D12_SHADER_VISIBILITY_ALL);

    CombineRootSignatureFlags(rootSignatureFlags, rootParameters);

    const StaticSampler staticSamples[] =
    {
        StaticSampler(0, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP),
        StaticSampler(1, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP),
        StaticSampler(2, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP),
        StaticSampler(3, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP),
        StaticSampler(4, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 0, 16,
            D3D12_COMPARISON_FUNC_LESS_EQUAL)
    };

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
    rootSignatureDescription.Init_1_1(
        rootParameters.size(), rootParameters.data(),
        _countof(staticSamples), staticSamples,
        rootSignatureFlags);

    this->SetRootSignatureDesc(rootSignatureDescription.Desc_1_1, featureData.HighestVersion);

    this->GetRootSignature()->SetName(L"Common Root Signature");
}

void CommonRootSignature::Bind(CommandList& commandList) const
{
    commandList.SetGraphicsAndComputeRootSignature(*this);

    for (UINT i = 0; i < MATERIAL_SRVS_COUNT; ++i)
    {
        commandList.SetShaderResourceView(RootParameters::MaterialSRVs, i,
            *m_EmptySRV.m_Resource,
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
            m_EmptySRV.m_FirstSubresource, m_EmptySRV.m_NumSubresources,
            m_EmptySRV.GetDescOrNullptr()
        );
    }

    for (UINT i = 0; i < PIPELINE_SRVS_COUNT; ++i)
    {
        commandList.SetShaderResourceView(RootParameters::PipelineSRVs, i,
            *m_EmptySRV.m_Resource,
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
            m_EmptySRV.m_FirstSubresource, m_EmptySRV.m_NumSubresources,
            m_EmptySRV.GetDescOrNullptr()
        );
    }

    for (UINT i = 0; i < UAVS_COUNT; ++i)
    {
        commandList.SetUnorderedAccessView(RootParameters::UAVs, i,
            *m_EmptyUAV.m_Resource,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            m_EmptyUAV.m_FirstSubresource, m_EmptySRV.m_NumSubresources,
            m_EmptyUAV.GetDescOrNullptr()
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

void CommonRootSignature::SetComputeConstantBuffer(CommandList& commandList, size_t size, const void* data) const
{
    commandList.SetComputeDynamicConstantBuffer(RootParameters::MaterialCBuffer, size, data);
}

void CommonRootSignature::SetGraphicsRootConstants(CommandList& commandList, size_t size, const void* data) const
{
    commandList.SetGraphics32BitConstants(RootParameters::Constants, static_cast<uint32_t>(size) / sizeof(uint32_t), data);
}
void CommonRootSignature::SetComputeRootConstants(CommandList& commandList, size_t size, const void* data) const
{
    commandList.SetCompute32BitConstants(RootParameters::Constants, static_cast<uint32_t>(size) / sizeof(uint32_t), data);
}

void CommonRootSignature::SetPipelineShaderResourceView(CommandList& commandList, UINT index, const ShaderResourceView& srv) const
{
    Assert(index < PIPELINE_SRVS_COUNT, "Pipeline SRV index is out of bounds.");

    if (srv.m_Resource->AreAutoBarriersEnabled())
    {
        commandList.SetShaderResourceView(RootParameters::PipelineSRVs,
            index,
            *srv.m_Resource,
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
            srv.m_FirstSubresource, srv.m_NumSubresources,
            srv.GetDescOrNullptr()
        );
    }
    else
    {
        commandList.SetShaderResourceView(RootParameters::PipelineSRVs,
            index,
            *srv.m_Resource,
            srv.m_FirstSubresource, srv.m_NumSubresources,
            srv.GetDescOrNullptr()
        );
    }
}

void CommonRootSignature::SetMaterialShaderResourceView(CommandList& commandList, UINT index, const ShaderResourceView& srv) const
{
    Assert(index < MATERIAL_SRVS_COUNT, "Material SRV index is out of bounds.");

    if (srv.m_Resource->AreAutoBarriersEnabled())
    {
        commandList.SetShaderResourceView(RootParameters::MaterialSRVs,
            index,
            *srv.m_Resource,
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
            srv.m_FirstSubresource, srv.m_NumSubresources,
            srv.GetDescOrNullptr()
        );
    }
    else
    {
        commandList.SetShaderResourceView(RootParameters::MaterialSRVs,
            index,
            *srv.m_Resource,
            srv.m_FirstSubresource, srv.m_NumSubresources,
            srv.GetDescOrNullptr()
        );
    }
}

void CommonRootSignature::SetComputeShaderResourceView(CommandList& commandList, UINT index, const ShaderResourceView& srv) const
{
    Assert(index < MATERIAL_SRVS_COUNT, "Compute SRV index is out of bounds.");

    if (srv.m_Resource->AreAutoBarriersEnabled())
    {
        commandList.SetShaderResourceView(RootParameters::MaterialSRVs,
            index,
            *srv.m_Resource,
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
            srv.m_FirstSubresource, srv.m_NumSubresources,
            srv.GetDescOrNullptr()
        );
    }
    else
    {
        commandList.SetShaderResourceView(RootParameters::MaterialSRVs,
            index,
            *srv.m_Resource,
            srv.m_FirstSubresource, srv.m_NumSubresources,
            srv.GetDescOrNullptr()
        );
    }
}

void CommonRootSignature::SetUnorderedAccessView(CommandList& commandList, UINT index, const UnorderedAccessView& uav) const
{
    Assert(index < UAVS_COUNT, "UAV index is out of bounds.");

    if (uav.m_Resource->AreAutoBarriersEnabled())
    {
        commandList.SetUnorderedAccessView(RootParameters::UAVs,
            index,
            *uav.m_Resource,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            uav.m_FirstSubresource, uav.m_NumSubresources,
            uav.GetDescOrNullptr()
        );
    }
    else
    {
        commandList.SetUnorderedAccessView(RootParameters::UAVs,
            index,
            *uav.m_Resource,
            uav.m_FirstSubresource, uav.m_NumSubresources,
            uav.GetDescOrNullptr()
        );
    }
}

void CommonRootSignature::UnbindMaterialShaderResourceViews(CommandList& commandList)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.PlaneSlice = 0;
    srvDesc.Texture2D.ResourceMinLODClamp = 0;

    for (UINT i = 0; i < MATERIAL_SRVS_COUNT; ++i)
    {
        commandList.SetShaderResourceView(RootParameters::MaterialSRVs,
            i,
            m_NullTexture,
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
            0, 1,
            &srvDesc
        );
    }
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
