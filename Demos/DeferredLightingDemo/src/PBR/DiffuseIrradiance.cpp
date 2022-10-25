#include <PBR/DiffuseIrradiance.h>

#include <Framework/Mesh.h>
#include <DX12Library/CommandList.h>
#include <DX12Library/Helpers.h>
#include <DX12Library/RenderTarget.h>
#include <d3dx12.h>
#include <DX12Library/Texture.h>
#include <DX12Library/Cubemap.h>
#include <DX12Library/ShaderUtils.h>

namespace
{
    struct Parameters
    {
        DirectX::XMFLOAT4 Forward;
        DirectX::XMFLOAT4 Up;
    };
}

DiffuseIrradiance::DiffuseIrradiance(const std::shared_ptr<CommonRootSignature>& rootSignature, CommandList& commandList)
    : m_BlitMesh(Mesh::CreateBlitTriangle(commandList))
{
    auto shader = std::make_shared<Shader>(rootSignature,
        ShaderBlob(L"IBL_CubeMapSideBlit_VS.cso"),
        ShaderBlob(L"IBL_DiffuseIrradiance_PS.cso")
        );
    m_Material = Material::Create(shader);
}

void DiffuseIrradiance::SetSourceCubemap(CommandList& commandList, const std::shared_ptr<Texture>& texture)
{
    const auto sourceDesc = texture->GetD3D12ResourceDesc();

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = sourceDesc.Format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
    srvDesc.TextureCube.MipLevels = 1;
    srvDesc.TextureCube.MostDetailedMip = 0;
    srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;

    auto srv = ShaderResourceView(texture, 0, UINT_MAX, srvDesc);
    m_Material->SetShaderResourceView("source", srv);
}

void DiffuseIrradiance::SetRenderTarget(CommandList& commandList, RenderTarget& renderTarget, UINT texArrayIndex /*= -1*/)
{
    commandList.SetRenderTarget(renderTarget, texArrayIndex);
    commandList.SetAutomaticViewportAndScissorRect(renderTarget);
}

void DiffuseIrradiance::Draw(CommandList& commandList, uint32_t cubemapSideIndex)
{
    const auto orientation = Cubemap::SIDE_ORIENTATIONS[cubemapSideIndex];
    Parameters parameters{};
    XMStoreFloat4(&parameters.Forward, orientation.Forward);
    XMStoreFloat4(&parameters.Up, orientation.Up);
    m_Material->SetAllVariables(parameters);

    m_Material->Bind(commandList);
    m_BlitMesh->Draw(commandList);
    m_Material->Unbind(commandList);
}
