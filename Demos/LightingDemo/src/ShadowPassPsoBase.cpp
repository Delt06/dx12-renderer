#include <ShadowPassPsoBase.h>

#include <DX12Library/CommandList.h>
#include <Framework/GameObject.h>
#include <DX12Library/Helpers.h>
#include <Framework/Mesh.h>
#include <Framework/Model.h>
#include <DX12Library/ShaderUtils.h>

using namespace DirectX;
using namespace Microsoft::WRL;

ShadowPassPsoBase::ShadowPassPsoBase(const std::shared_ptr<CommonRootSignature>& rootSignature, UINT resolution)
    : m_ShadowPassParameters{}
    , m_Resolution(resolution)
    , m_Viewport(CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(m_Resolution), static_cast<float>(m_Resolution)))
    , m_ScissorRect(CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX))
    , m_RootSignature(rootSignature)
{
    auto shader = std::make_shared<Shader>(rootSignature,
        ShaderBlob(L"LightingDemo_ShadowCaster_VS.cso"),
        ShaderBlob(L"LightingDemo_ShadowCaster_PS.cso"),
        [](PipelineStateBuilder& builder)
        {
            // ColorMask 0
            auto blendDesc = CD3DX12_BLEND_DESC(CD3DX12_DEFAULT());
            {
                auto& renderTargetBlendDesc = blendDesc.RenderTarget[0];
                renderTargetBlendDesc.RenderTargetWriteMask = 0;
            }
            builder.WithBlend(blendDesc);

            auto depthStencilDesc = CD3DX12_DEPTH_STENCIL_DESC(CD3DX12_DEFAULT());
            builder.WithDepthStencil(depthStencilDesc);

            auto rasterizerDesc = CD3DX12_RASTERIZER_DESC(CD3DX12_DEFAULT());
            builder.WithRasterizer(rasterizerDesc);
        }
    );
    m_Material = Material::Create(shader);
}

ShadowPassPsoBase::~ShadowPassPsoBase() = default;

void ShadowPassPsoBase::Begin(CommandList& commandList) const
{
    commandList.SetViewport(m_Viewport);
    commandList.SetScissorRect(m_ScissorRect);

    m_Material->BeginBatch(commandList);
}

void ShadowPassPsoBase::End(CommandList& commandList) const
{
    m_Material->EndBatch(commandList);
}

void ShadowPassPsoBase::SetBias(const float depthBias, const float normalBias)
{
    m_ShadowPassParameters.Bias.x = -depthBias;
    m_ShadowPassParameters.Bias.y = -normalBias;
}

void ShadowPassPsoBase::DrawToShadowMap(CommandList& commandList, const GameObject& gameObject) const
{
    ShadowPassParameters parameters = m_ShadowPassParameters;
    const auto worldMatrix = gameObject.GetWorldMatrix();
    parameters.InverseTransposeModel = DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, worldMatrix));
    parameters.Model = worldMatrix;

    m_RootSignature->SetModelConstantBuffer(commandList, parameters);

    for (auto& mesh : gameObject.GetModel()->GetMeshes())
    {
        mesh->Draw(commandList);
    }
}

XMMATRIX ShadowPassPsoBase::GetShadowViewProjectionMatrix() const
{
    return m_ShadowPassParameters.ViewProjection;
}
