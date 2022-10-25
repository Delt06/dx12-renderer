#include <DirectXMath.h>

#include <PostFxPso.h>
#include <Framework/Mesh.h>
#include <DX12Library/CommandList.h>
#include <DX12Library/Helpers.h>
#include <d3dx12.h>
#include <DX12Library/Texture.h>
#include <DX12Library/Camera.h>
#include <DX12Library/ShaderUtils.h>

using namespace DirectX;

PostFxPso::PostFxPso(const std::shared_ptr<CommonRootSignature>& rootSignature, CommandList& commandList)
{
    m_BlitTriangle = Mesh::CreateBlitTriangle(commandList);

    auto shader = std::make_shared<Shader>(rootSignature,
        ShaderBlob(L"LightingDemo_Blit_VS.cso"),
        ShaderBlob(L"LightingDemo_PostFX_PS.cso"),
        [](PipelineStateBuilder& builder)
        {
            builder.WithRasterizer(CD3DX12_RASTERIZER_DESC(D3D12_FILL_MODE_SOLID, D3D12_CULL_MODE_BACK, FALSE, 0, 0,
                0, TRUE, FALSE, FALSE, 0,
                D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF));

            auto depthStencilDesc = CD3DX12_DEPTH_STENCIL_DESC(CD3DX12_DEFAULT());
            {
                depthStencilDesc.DepthEnable = false;
            }
            builder.WithDepthStencil(depthStencilDesc);
        }
    );
    m_Material = Material::Create(shader);
}

void PostFxPso::SetSourceColorTexture(CommandList& commandList, const std::shared_ptr<Texture>& texture)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.PlaneSlice = 0;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

    m_Material->SetShaderResourceView("sourceColorTexture", ShaderResourceView(texture, srvDesc));
}

void PostFxPso::SetSourceDepthTexture(CommandList& commandList, const std::shared_ptr<Texture>& texture)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.PlaneSlice = 0;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

    m_Material->SetShaderResourceView("sourceDepthTexture", ShaderResourceView(texture, srvDesc));
}

void PostFxPso::SetParameters(CommandList& commandList, const PostFxParameters& parameters)
{
    m_Material->SetAllVariables(parameters);
}

void PostFxPso::Blit(CommandList& commandList)
{
    m_Material->Bind(commandList);
    m_BlitTriangle->Draw(commandList);
    m_Material->Unbind(commandList);
}
