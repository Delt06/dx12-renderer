#pragma once

#include <d3dx12.h>

#include <DX12Library/RootSignature.h>
#include <DX12Library/CommandList.h>
#include <DX12Library/Resource.h>
#include <DX12Library/DescriptorAllocation.h>

#include <vector>

#include "ShaderResourceView.h"
#include "UnorderedAccessView.h"

class CommonRootSignature final : public RootSignature
{
public:
    explicit CommonRootSignature(const std::shared_ptr<Resource>& emptyResource);

    void Bind(CommandList& commandList) const;

    void SetPipelineConstantBuffer(CommandList& commandList, size_t size, const void* data) const;

    template<typename T>
    inline void SetPipelineConstantBuffer(CommandList& commandList, const T& data) const
    {
        SetPipelineConstantBuffer(commandList, sizeof(T), &data);
    }

    void SetMaterialConstantBuffer(CommandList& commandList, size_t size, const void* data) const;

    void SetModelConstantBuffer(CommandList& commandList, size_t size, const void* data) const;

    template<typename T>
    inline void SetModelConstantBuffer(CommandList& commandList, const T& data) const
    {
        SetModelConstantBuffer(commandList, sizeof(T), &data);
    }

    void SetComputeConstantBuffer(CommandList& commandList, size_t size, const void* data) const;

    template<typename T>
    inline void SetComputeConstantBuffer(CommandList& commandList, const T& data) const
    {
        SetComputeConstantBuffer(commandList, sizeof(T), &data);
    }

    void SetPipelineShaderResourceView(CommandList& commandList, UINT index, const ShaderResourceView& srv) const;

    void SetMaterialShaderResourceView(CommandList& commandList, UINT index, const ShaderResourceView& srv) const;

    void SetComputeShaderResourceView(CommandList& commandList, UINT index, const ShaderResourceView& srv) const;

    void SetUnorderedAccessView(CommandList& commandList, UINT index, const UnorderedAccessView& uav) const;

    void UnbindMaterialShaderResourceViews(CommandList& commandList);

    static constexpr UINT MATERIAL_REGISTER_SPACE = 0u;
    static constexpr UINT MODEL_REGISTER_SPACE = 1u;
    static constexpr UINT PIPELINE_REGISTER_SPACE = 2u;

    struct RootParameters
    {
        enum
        {
            // sorted by the change frequency (high to low)
            ModelCBuffer,

            MaterialCBuffer,
            MaterialSRVs,

            PipelineCBuffer,
            PipelineSRVs,

            UAVs,

            NumRootParameters,
        };
    };

private:

    using RootParameter = CD3DX12_ROOT_PARAMETER1;
    using DescriptorRange = CD3DX12_DESCRIPTOR_RANGE1;
    using StaticSampler = CD3DX12_STATIC_SAMPLER_DESC;

    void CombineRootSignatureFlags(D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags, const std::vector<RootParameter>& rootParameters);
    bool CheckRootParametersVisiblity(const std::vector<RootParameter>& rootParameters, D3D12_SHADER_VISIBILITY param2);

    ShaderResourceView m_EmptySRV;
    UnorderedAccessView m_EmptyUAV;
    Texture m_NullTexture;
};
