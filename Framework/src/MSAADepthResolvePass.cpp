#include "MSAADepthResolvePass.h"
#include <DX12Library/Helpers.h>

#include <Framework/MSAADepthResolve_CS.h>

namespace
{
    static constexpr uint32_t THREAD_GROUP_SIZE = 16u;
}

MSAADepthResolvePass::MSAADepthResolvePass(const std::shared_ptr<CommonRootSignature>& rootSignature)
    : m_RootSignature(rootSignature)
    , m_ComputeShader(rootSignature, ShaderBlob(ShaderBytecode_MSAADepthResolve_CS, sizeof(ShaderBytecode_MSAADepthResolve_CS)))
{}

void MSAADepthResolvePass::Resolve(CommandList& commandList, const std::shared_ptr<Texture>& source, const std::shared_ptr<Texture>& destination) const
{
    const auto sourceDesc = source->GetD3D12ResourceDesc();
    const auto destinationDesc = destination->GetD3D12ResourceDesc();
    if (sourceDesc.Width != destinationDesc.Width || sourceDesc.Height != destinationDesc.Height)
    {
        throw std::exception("Source and destination sizes do not match.");
    }

    m_ComputeShader.Bind(commandList);

    D3D12_SHADER_RESOURCE_VIEW_DESC sourceSrvDesc{};
    sourceSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
    sourceSrvDesc.Format = DXGI_FORMAT_R32_FLOAT;
    sourceSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    m_RootSignature->SetMaterialShaderResourceView(commandList, 0, ShaderResourceView(source, sourceSrvDesc));
    m_RootSignature->SetUnorderedAccessView(commandList, 0, UnorderedAccessView(destination));

    commandList.Dispatch(
        Math::DivideByMultiple(static_cast<uint32_t>(sourceDesc.Width), THREAD_GROUP_SIZE),
        Math::DivideByMultiple(sourceDesc.Height, THREAD_GROUP_SIZE)
    );
}
