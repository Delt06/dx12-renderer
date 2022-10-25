#include <ParticleSystemPso.h>

#include <DX12Library/Helpers.h>
#include <d3d12.h>
#include <d3dx12.h>
#include <Framework/Mesh.h>
#include <DX12Library/Texture.h>

#include <DX12Library/ShaderUtils.h>

using namespace Microsoft::WRL;

namespace
{
    namespace RootParameters
    {
        enum RootParameters
        {
            MatricesCb = 0,
            Textures,
            NumRootParameters,
        };
    }

    constexpr UINT INSTANCE_DATA_INPUT_SLOT = 1;

    constexpr D3D12_INPUT_ELEMENT_DESC INSTANCE_INPUT_ELEMENTS[] = {
        {
            "INSTANCE_PIVOT", 0, DXGI_FORMAT_R32G32B32_FLOAT, INSTANCE_DATA_INPUT_SLOT, D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1
        },
        {
            "INSTANCE_COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, INSTANCE_DATA_INPUT_SLOT, D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1
        },
        {
            "INSTANCE_SCALE", 0, DXGI_FORMAT_R32_FLOAT, INSTANCE_DATA_INPUT_SLOT, D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1
        },
    };

    constexpr size_t INSTANCE_INPUT_ELEMENT_COUNT = _countof(INSTANCE_INPUT_ELEMENTS);
}

ParticleSystemPso::ParticleSystemPso(const std::shared_ptr<CommonRootSignature>& rootSignature, CommandList& commandList)
{
    m_QuadMesh = Mesh::CreateVerticalQuad(commandList);

    m_Texture = std::make_shared<Texture>();
    commandList.LoadTextureFromFile(*m_Texture, L"Assets/Textures/particle.png", TextureUsageType::Albedo);

    auto shader = std::make_shared<Shader>(rootSignature,
        ShaderBlob(L"LightingDemo_ParticleSystem_VS.cso"),
        ShaderBlob(L"LightingDemo_ParticleSystem_PS.cso"),
        [](PipelineStateBuilder& builder)
        {
            constexpr size_t inputElementsCount = VertexAttributes::INPUT_ELEMENT_COUNT + INSTANCE_INPUT_ELEMENT_COUNT;
            std::vector<D3D12_INPUT_ELEMENT_DESC> inputElements(inputElementsCount);
            std::copy_n(VertexAttributes::INPUT_ELEMENTS, VertexAttributes::INPUT_ELEMENT_COUNT, inputElements.data());
            std::copy_n(INSTANCE_INPUT_ELEMENTS, INSTANCE_INPUT_ELEMENT_COUNT,
                inputElements.data() + VertexAttributes::INPUT_ELEMENT_COUNT
            );

            builder.WithInputLayout(inputElements);

            // alpha blending
            auto blendDesc = CD3DX12_BLEND_DESC(CD3DX12_DEFAULT());
            {
                auto& renderTargetBlendDesc = blendDesc.RenderTarget[0];
                renderTargetBlendDesc.BlendEnable = TRUE;
                renderTargetBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
                renderTargetBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
                renderTargetBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
                renderTargetBlendDesc.SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
                renderTargetBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
                renderTargetBlendDesc.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
            }
            builder.WithBlend(blendDesc);

            // disable depth write, keep depth check
            auto depthStencilDesc = CD3DX12_DEPTH_STENCIL_DESC(CD3DX12_DEFAULT());
            {
                depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
            }
            builder.WithDepthStencil(depthStencilDesc);
        }
    );
    m_Material = Material::Create(shader);
}

void ParticleSystemPso::Begin(CommandList& commandList) const
{
    m_Material->BeginBatch(commandList);
    m_Material->SetShaderResourceView("albedoTexture", ShaderResourceView(m_Texture));
    m_Material->UploadUniforms(commandList);
    commandList.SetVertexBuffer(INSTANCE_DATA_INPUT_SLOT, m_InstanceDataVertexBuffer);
}

void ParticleSystemPso::End(CommandList& commandList) const
{
    m_Material->EndBatch(commandList);
}

void ParticleSystemPso::UploadInstanceData(CommandList& commandList,
    const ParticleInstanceData* instanceData, const uint32_t instancesCount)
{
    m_InstancesCount = instancesCount;
    commandList.CopyVertexBuffer(m_InstanceDataVertexBuffer, instancesCount, sizeof(ParticleInstanceData),
        instanceData);
}

void ParticleSystemPso::Draw(CommandList& commandList) const
{
    m_QuadMesh->Draw(commandList, m_InstancesCount);
}
