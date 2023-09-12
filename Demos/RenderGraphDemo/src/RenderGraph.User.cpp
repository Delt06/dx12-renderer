#include "RenderGraph.User.h"

#include <DX12Library/StructuredBuffer.h>

#include <Framework/Blit_VS.h>
#include <Framework/ComputeShader.h>
#include <Framework/Material.h>
#include <Framework/Mesh.h>
#include <Framework/Shader.h>

#include "ResourceIds.User.h"

namespace
{
    constexpr FLOAT CLEAR_COLOR[] = { 138.0f / 255.0f, 82.0f / 255.0f, 52.0f / 255.0f, 1.0f };
    constexpr DXGI_FORMAT BACK_BUFFER_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
}

std::unique_ptr<RenderGraph::RenderGraphRoot> RenderGraph::User::Create(
    const std::shared_ptr<CommonRootSignature>& rootSignature,
    CommandList& commandList
)
{
    using namespace RenderGraph;
    using namespace DirectX;

    std::vector<std::unique_ptr<RenderPass>> renderPasses;
    renderPasses.emplace_back(RenderPass::Create(
        L"Setup",
        {

        },
        {
            { ::ResourceIds::User::TempRenderTarget3, OutputType::RenderTarget },
            { ::ResourceIds::User::SetupFinishedToken, OutputType::Token }
        },
        [rootSignature](const auto& metadata, auto& commandList)
        {
            rootSignature->Bind(commandList);
        }
    ));
    renderPasses.emplace_back(RenderPass::Create(
        L"Color Split Buffer Count Reset",
        {
            { ::ResourceIds::User::SetupFinishedToken, InputType::Token }
        },
    {
            { ::ResourceIds::User::ColorSplitBufferInitToken, OutputType::Token },
            { ::ResourceIds::User::ColorSplitBuffer, OutputType::CopyDestination }
    },
        [](const auto& context, auto& commandList)
        {
            const auto& pBuffer = context.m_ResourcePool->GetBuffer(::ResourceIds::User::ColorSplitBuffer);
            commandList.CopyByteAddressBuffer<uint32_t>(pBuffer->GetCounterBuffer(), 0u);
        }
    ));

    {
        auto pColorSplitBufferShader = std::make_shared<ComputeShader>(rootSignature,
            ShaderBlob(L"ColorSplitBuffer_CS.cso")
        );
        renderPasses.emplace_back(RenderPass::Create(
            L"Color Split Compute",
            {
                { ::ResourceIds::User::ColorSplitBufferInitToken, InputType::Token }
            },
            {
                { ::ResourceIds::User::ColorSplitBuffer, OutputType::UnorderedAccess },
            },
            [rootSignature, pColorSplitBufferShader](const auto& context, auto& commandList)
            {
                pColorSplitBufferShader->Bind(commandList);

                const auto& pBuffer = context.m_ResourcePool->GetBuffer(::ResourceIds::User::ColorSplitBuffer);
                rootSignature->SetUnorderedAccessView(commandList, 0,
                    UnorderedAccessView(pBuffer)
                );
                rootSignature->SetComputeConstantBuffer<uint64_t>(commandList, context.m_Metadata.m_FrameIndex);

                commandList.Dispatch(1, 1, 1);
            }
        ));
    }

    renderPasses.emplace_back(RenderPass::Create(
        L"Copy Temp RT",
        {
            { ::ResourceIds::User::TempRenderTarget3, InputType::CopySource }
        },
        {
            { ::ResourceIds::User::TempRenderTarget, OutputType::CopyDestination },
            { ::ResourceIds::User::TempRenderTargetReadyToken, OutputType::Token },
        },
        [](const auto& context, auto& commandList)
        {
            const auto& src = context.m_ResourcePool->GetResource(::ResourceIds::User::TempRenderTarget3);
            const auto& dst = context.m_ResourcePool->GetResource(::ResourceIds::User::TempRenderTarget);
            commandList.CopyResource(dst, src);
        }
    ));

    {

        auto pMesh = Mesh::CreateVerticalQuad(commandList, 0.5f, 0.5f);
        auto pShader = std::make_shared<Shader>(rootSignature,
            ShaderBlob(ShaderBytecode_Blit_VS, sizeof(ShaderBytecode_Blit_VS)),
            ShaderBlob(L"WhiteShader_PS.cso")
        );
        renderPasses.emplace_back(RenderPass::Create(
            L"Draw Quad",
            {
                { ::ResourceIds::User::TempRenderTargetReadyToken, InputType::Token },
            },
            {
                { ::ResourceIds::User::TempRenderTarget, OutputType::RenderTarget },
            },
            [pMesh, pShader](const auto& context, auto& commandList)
            {
                pShader->Bind(commandList);

                pMesh->Bind(commandList);
                pMesh->Draw(commandList);
            }
        ));
    }
    renderPasses.emplace_back(RenderPass::Create(
        L"Draw Quad",
        {
            { ::ResourceIds::User::TempRenderTarget3, InputType::CopySource }
        },
        {
            { ::ResourceIds::User::TempRenderTarget2, OutputType::RenderTarget },
        },
        [](const auto& context, auto& commandList)
        {

        }
    ));

    {
        auto pBlitMesh = Mesh::CreateBlitTriangle(commandList);
        auto pShader = std::make_shared<Shader>(rootSignature,
            ShaderBlob(ShaderBytecode_Blit_VS, sizeof(ShaderBytecode_Blit_VS)),
            ShaderBlob(L"PostProcessing_PS.cso")
        );
        auto pMaterial = std::make_shared<Material>(pShader);
        renderPasses.emplace_back(RenderPass::Create(
            L"Post-Processing",
            {
                { ::ResourceIds::User::TempRenderTarget, InputType::ShaderResource },
                { ::ResourceIds::User::ColorSplitBuffer, InputType::ShaderResource },
            },
            {
                { RenderGraph::ResourceIds::GraphOutput, OutputType::RenderTarget }
            },
            [pBlitMesh, pMaterial](const auto& context, auto& commandList)
            {
                const auto& pTempRt = context.m_ResourcePool->GetTexture(::ResourceIds::User::TempRenderTarget);
                const auto& pColorSplitBuffer = context.m_ResourcePool->GetBuffer(::ResourceIds::User::ColorSplitBuffer);
                const auto& ppColorSplitBufferCounter = pColorSplitBuffer->GetCounterBufferPtr();

                pMaterial->SetShaderResourceView("_Source", ShaderResourceView(pTempRt));
                pMaterial->SetShaderResourceView("_ColorSplitBuffer", ShaderResourceView(pColorSplitBuffer));
                pMaterial->SetShaderResourceView("_ColorSplitBufferCounter", ShaderResourceView(ppColorSplitBufferCounter));

                auto desc = pTempRt->GetD3D12ResourceDesc();
                pMaterial->SetVariable("_TexelSize", XMFLOAT2{ 1.0f / desc.Width, 1.0f / desc.Height });
                pMaterial->SetVariable("_Time", static_cast<float>(context.m_Metadata.m_Time));
                pMaterial->Bind(commandList);

                pBlitMesh->Draw(commandList);
            }
        ));
    }


    renderPasses.emplace_back(RenderPass::Create(
        L"Useless Pass",
        {
            { ::ResourceIds::User::TempRenderTarget3, InputType::CopySource }
        },
        {
            { ::ResourceIds::User::TempRenderTarget2, OutputType::RenderTarget },
        },
        [](const auto& context, auto& commandList)
        {

        }
    ));

    RenderMetadataExpression<uint32_t> renderWidthExpression = [](const RenderMetadata& metadata) { return metadata.m_ScreenWidth; };
    RenderMetadataExpression<uint32_t> renderHeightExpression = [](const RenderMetadata& metadata) { return metadata.m_ScreenHeight; };

    std::vector<TextureDescription> textures = {
        {::ResourceIds::User::TempRenderTarget, renderWidthExpression, renderHeightExpression, BACK_BUFFER_FORMAT, CLEAR_COLOR, ResourceInitAction::CopyDestination},
        {::ResourceIds::User::TempRenderTarget2, renderWidthExpression, renderHeightExpression, BACK_BUFFER_FORMAT, CLEAR_COLOR, ResourceInitAction::Clear},
        {::ResourceIds::User::TempRenderTarget3, renderWidthExpression, renderHeightExpression, BACK_BUFFER_FORMAT, CLEAR_COLOR, ResourceInitAction::Clear},
        {RenderGraph::ResourceIds::GraphOutput, renderWidthExpression, renderHeightExpression, Window::BUFFER_FORMAT_SRGB, CLEAR_COLOR, ResourceInitAction::Discard},
    };

    struct ColorSplitEntry
    {
        XMFLOAT2 m_UvOffset;
        float _Padding1[2];
        XMFLOAT3 m_ChannelMask;
        float _Padding2[1];
    };

    std::vector<BufferDescription> buffers =
    {
        BufferDescription{ ::ResourceIds::User::ColorSplitBuffer, [](const RenderMetadata& metadata) { return 32LU; }, sizeof(ColorSplitEntry), ResourceInitAction::CopyDestination },
    };

    std::vector<TokenDescription> tokens =
    {
        {::ResourceIds::User::SetupFinishedToken},
        {::ResourceIds::User::ColorSplitBufferInitToken},
        {::ResourceIds::User::TempRenderTargetReadyToken},
    };

    return std::make_unique<RenderGraphRoot>(std::move(renderPasses), std::move(textures), std::move(buffers), std::move(tokens));
}
