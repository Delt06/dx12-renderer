#include "RenderGraph.User.h"

#include <DirectXMath.h>

#include <DX12Library/StructuredBuffer.h>

#include <Framework/Blit_VS.h>
#include <Framework/Light.h>
#include <Framework/ComputeShader.h>
#include <Framework/Material.h>
#include <Framework/Mesh.h>
#include <Framework/Shader.h>
#include <Framework/SharedUploadBuffer.h>

#include "MeshletsDemo.h"
#include "Framework/Model.h"

using namespace DirectX;

namespace ResourceIds
{
    class User
    {
    public:
        static inline const RenderGraph::ResourceId DepthBuffer = RenderGraph::ResourceIds::GetResourceId(L"DepthBuffer");

        static inline const RenderGraph::ResourceId CommonVertexBuffer = RenderGraph::ResourceIds::GetResourceId(L"CommonVertexBuffer");
        static inline const RenderGraph::ResourceId CommonIndexBuffer = RenderGraph::ResourceIds::GetResourceId(L"CommonIndexBuffer");
        static inline const RenderGraph::ResourceId MeshletsBuffer = RenderGraph::ResourceIds::GetResourceId(L"MeshletsBuffer");
        static inline const RenderGraph::ResourceId TransformsBuffer = RenderGraph::ResourceIds::GetResourceId(L"TransformsBuffer");

        static inline const RenderGraph::ResourceId SetupFinishedToken = RenderGraph::ResourceIds::GetResourceId(L"SetupFinishedToken");
    };
}

namespace
{
    constexpr FLOAT CLEAR_COLOR[] = { 138.0f / 255.0f, 82.0f / 255.0f, 52.0f / 255.0f, 1.0f };
    constexpr DXGI_FORMAT BACK_BUFFER_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    constexpr DXGI_FORMAT DEPTH_BUFFER_FORMAT = DXGI_FORMAT_D32_FLOAT;
}

namespace CBuffer
{
    struct Pipeline
    {
        XMMATRIX g_Pipeline_View;
        XMMATRIX g_Pipeline_Projection;
        XMMATRIX g_Pipeline_ViewProjection;

        XMFLOAT4 g_Pipeline_CameraPosition;

        XMMATRIX g_Pipeline_InverseView;
        XMMATRIX g_Pipeline_InverseProjection;

        XMFLOAT2 g_Pipeline_Screen_Resolution;
        XMFLOAT2 g_Pipeline_Screen_TexelSize;

        DirectionalLight g_Pipeline_DirectionalLight;
    };

    struct MeshletRootConstants
    {
        uint32_t g_Meshlet_Index;
    };
}

std::unique_ptr<RenderGraph::RenderGraphRoot> RenderGraph::User::Create(
    MeshletsDemo& demo,
    const std::shared_ptr<CommonRootSignature>& pRootSignature,
    CommandList& commandList
)
{
    using namespace RenderGraph;
    using namespace DirectX;

    const auto pSharedUploadBuffer = std::make_shared<SharedUploadBuffer>();

    std::vector<std::unique_ptr<RenderPass>> renderPasses;
    renderPasses.emplace_back(RenderPass::Create(
        L"Setup",
        {

        },
        {
            { ::ResourceIds::User::SetupFinishedToken, OutputType::Token }
        },
        [&demo, pRootSignature, pSharedUploadBuffer](const RenderContext& context, CommandList& commandList)
        {
            const auto& camera = demo.m_Camera;
            const XMMATRIX viewMatrix = camera.GetViewMatrix();
            const XMMATRIX projectionMatrix = camera.GetProjectionMatrix();
            const XMMATRIX viewProjectionMatrix = viewMatrix * projectionMatrix;

            pRootSignature->Bind(commandList);
            CBuffer::Pipeline pipelineCBuffer;
            pipelineCBuffer.g_Pipeline_View = viewMatrix;
            pipelineCBuffer.g_Pipeline_Projection = projectionMatrix;
            pipelineCBuffer.g_Pipeline_ViewProjection = viewProjectionMatrix;

            XMStoreFloat4(&pipelineCBuffer.g_Pipeline_CameraPosition, camera.GetTranslation());

            pipelineCBuffer.g_Pipeline_InverseView = XMMatrixInverse(nullptr, viewMatrix);
            pipelineCBuffer.g_Pipeline_InverseProjection = XMMatrixInverse(nullptr, projectionMatrix);

            pipelineCBuffer.g_Pipeline_Screen_Resolution = { static_cast<float>(context.m_Metadata.m_ScreenWidth), static_cast<float>(context.m_Metadata.m_ScreenHeight) };
            pipelineCBuffer.g_Pipeline_Screen_TexelSize = { 1.0f / pipelineCBuffer.g_Pipeline_Screen_Resolution.x, 1.0f / pipelineCBuffer.g_Pipeline_Screen_Resolution.y };

            pipelineCBuffer.g_Pipeline_DirectionalLight = demo.m_DirectionalLight;

            pRootSignature->Bind(commandList);
            pRootSignature->SetPipelineConstantBuffer(commandList, pipelineCBuffer);

            pSharedUploadBuffer->BeginFrame();
        }
    ));

    renderPasses.emplace_back(RenderPass::Create(
        L"Prepare Common Buffers",
        {
            { ::ResourceIds::User::SetupFinishedToken, InputType::Token }
        },
        {
            { ::ResourceIds::User::CommonVertexBuffer, OutputType::CopyDestination },
            { ::ResourceIds::User::CommonIndexBuffer, OutputType::CopyDestination },
            { ::ResourceIds::User::MeshletsBuffer, OutputType::CopyDestination },
            { ::ResourceIds::User::TransformsBuffer, OutputType::CopyDestination },
        },
        [&demo, pSharedUploadBuffer](const RenderContext& context, CommandList& commandList)
        {
            const auto& pCommonVertexBuffer = context.m_ResourcePool->GetBuffer(::ResourceIds::User::CommonVertexBuffer);
            const auto& pCommonIndexBuffer = context.m_ResourcePool->GetBuffer(::ResourceIds::User::CommonIndexBuffer);
            const auto& pMeshletsBuffer = context.m_ResourcePool->GetBuffer(::ResourceIds::User::MeshletsBuffer);
            const auto& pTransformsBuffer = context.m_ResourcePool->GetBuffer(::ResourceIds::User::TransformsBuffer);

            const auto& meshletSet = demo.m_MeshletsBuffer;

            const auto& meshPrototype = meshletSet.m_MeshPrototype;
            pSharedUploadBuffer->Upload(commandList, *pCommonVertexBuffer, meshPrototype.m_Vertices);
            pSharedUploadBuffer->Upload(commandList, *pCommonIndexBuffer, meshPrototype.m_Indices);

            pSharedUploadBuffer->Upload(commandList, *pMeshletsBuffer, meshletSet.m_Meshlets);

            pSharedUploadBuffer->Upload(commandList, *pTransformsBuffer, demo.m_TransformsBuffer);
        }
    ));

    renderPasses.emplace_back(RenderPass::Create(
        L"Draw Geometries",
        {
            { ::ResourceIds::User::CommonVertexBuffer, InputType::ShaderResource },
            { ::ResourceIds::User::CommonIndexBuffer, InputType::ShaderResource },
            { ::ResourceIds::User::MeshletsBuffer, InputType::ShaderResource },
            { ::ResourceIds::User::TransformsBuffer, InputType::ShaderResource },
        },
        {
            { ResourceIds::GraphOutput, OutputType::RenderTarget },
            { ::ResourceIds::User::DepthBuffer, OutputType::DepthWrite }
        },
        [&demo, pRootSignature](const RenderContext& context, CommandList& commandList)
        {
            if (demo.m_GameObjects.size() == 0)
            {
                return;
            }

            const auto& pCommonVertexBuffer = context.m_ResourcePool->GetBuffer(::ResourceIds::User::CommonVertexBuffer);
            const auto& pCommonIndexBuffer = context.m_ResourcePool->GetBuffer(::ResourceIds::User::CommonIndexBuffer);
            const auto& pMeshletsBuffer = context.m_ResourcePool->GetBuffer(::ResourceIds::User::MeshletsBuffer);
            const auto& pTransformsBuffer = context.m_ResourcePool->GetBuffer(::ResourceIds::User::TransformsBuffer);

            pRootSignature->SetPipelineShaderResourceView(commandList, 0, ShaderResourceView(pCommonVertexBuffer));
            pRootSignature->SetPipelineShaderResourceView(commandList, 1, ShaderResourceView(pCommonIndexBuffer));
            pRootSignature->SetPipelineShaderResourceView(commandList, 2, ShaderResourceView(pMeshletsBuffer));
            pRootSignature->SetPipelineShaderResourceView(commandList, 3, ShaderResourceView(pTransformsBuffer));

            commandList.SetPrimitiveTopology(Mesh::PRIMITIVE_TOPOLOGY);

            for (auto& go : demo.m_GameObjects)
            {
                const auto& pMaterial = go.GetMaterial();

                pMaterial->Bind(commandList);

                for (auto& pMesh : go.GetModel()->GetMeshes())
                {
                    for (uint32_t localMeshletIndex = 0; localMeshletIndex < pMesh->m_MeshletsCount; ++localMeshletIndex)
                    {
                        const uint32_t absoluteMeshletIndex = pMesh->m_MeshletsOffset + localMeshletIndex;
                        CBuffer::MeshletRootConstants constants{};
                        constants.g_Meshlet_Index = absoluteMeshletIndex;
                        pRootSignature->SetGraphicsRootConstants(commandList, constants);

                        commandList.Draw(demo.m_MeshletsBuffer.m_Meshlets[absoluteMeshletIndex].m_IndexCount);
                    }
                }

                pMaterial->Unbind(commandList);
            }
        }
    ));

    const RenderMetadataExpression renderWidthExpression = [](const RenderMetadata& metadata) { return metadata.m_ScreenWidth; };
    const RenderMetadataExpression renderHeightExpression = [](const RenderMetadata& metadata) { return metadata.m_ScreenHeight; };


    std::vector textures = {
        TextureDescription{ ::ResourceIds::User::DepthBuffer, renderWidthExpression, renderHeightExpression, DEPTH_BUFFER_FORMAT, { 1.0f, 0u }, Clear },
        TextureDescription{ ResourceIds::GraphOutput, renderWidthExpression, renderHeightExpression, Window::BUFFER_FORMAT_SRGB, CLEAR_COLOR, Clear },
    };

    std::vector buffers =
    {
        BufferDescription{ ::ResourceIds::User::CommonVertexBuffer, [&demo](const auto&) { return demo.m_MeshletsBuffer.m_MeshPrototype.m_Vertices.size(); }, sizeof(VertexAttributes), CopyDestination },
        BufferDescription{ ::ResourceIds::User::CommonIndexBuffer, [&demo](const auto&) { return demo.m_MeshletsBuffer.m_MeshPrototype.m_Indices.size() * sizeof(uint16_t); }, 1, CopyDestination },
        BufferDescription{ ::ResourceIds::User::MeshletsBuffer, [&demo](const auto&) { return demo.m_MeshletsBuffer.m_Meshlets.size(); }, sizeof(Meshlet), CopyDestination },
        BufferDescription{ ::ResourceIds::User::TransformsBuffer, [&demo](const auto&) { return demo.m_TransformsBuffer.size(); }, sizeof(Transform), CopyDestination },
    };

    std::vector<TokenDescription> tokens =
    {
        { ::ResourceIds::User::SetupFinishedToken },
    };

    return std::make_unique<RenderGraphRoot>(std::move(renderPasses), std::move(textures), std::move(buffers), std::move(tokens));
}
