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

    struct Model
    {
        XMMATRIX g_Model_Model;
        XMMATRIX g_Model_InverseTransposeModel;
        uint32_t g_Model_Index;

        void Compute(const XMMATRIX& model)
        {
            g_Model_Model = model;
            g_Model_InverseTransposeModel = XMMatrixTranspose(XMMatrixInverse(nullptr, model));
        }
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

    const auto pShaderUploadBuffer = std::make_shared<SharedUploadBuffer>();

    std::vector<std::unique_ptr<RenderPass>> renderPasses;
    renderPasses.emplace_back(RenderPass::Create(
        L"Setup",
        {

        },
        {
            { ::ResourceIds::User::SetupFinishedToken, OutputType::Token }
        },
        [&demo, pRootSignature, pShaderUploadBuffer](const RenderContext& context, CommandList& commandList)
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

            pShaderUploadBuffer->BeginFrame();
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
        },
        [&demo, pShaderUploadBuffer](const RenderContext& context, CommandList& commandList)
        {
            if (demo.m_GameObjects.size() == 0)
            {
                return;
            }

            {
                const auto& pCommonVertexBuffer = context.m_ResourcePool->GetBuffer(::ResourceIds::User::CommonVertexBuffer);
                const auto& pCommonIndexBuffer = context.m_ResourcePool->GetBuffer(::ResourceIds::User::CommonIndexBuffer);

                const MeshPrototype& meshPrototype = demo.m_MeshletSets[0][0].m_MeshPrototype;

                pShaderUploadBuffer->Upload(commandList, *pCommonVertexBuffer, meshPrototype.m_Vertices);
                pShaderUploadBuffer->Upload(commandList, *pCommonIndexBuffer, meshPrototype.m_Indices);
            }

            {
                const auto& pMeshletsBuffer = context.m_ResourcePool->GetBuffer(::ResourceIds::User::MeshletsBuffer);

                const MeshletBuilder::MeshletSet& meshletSet = demo.m_MeshletSets[0][0];

                pShaderUploadBuffer->Upload(commandList, *pMeshletsBuffer, meshletSet.m_Meshlets);
            }
        }
    ));

    renderPasses.emplace_back(RenderPass::Create(
        L"Draw Geometries",
        {
            { ::ResourceIds::User::CommonVertexBuffer, InputType::ShaderResource },
            { ::ResourceIds::User::CommonIndexBuffer, InputType::ShaderResource },
            { ::ResourceIds::User::MeshletsBuffer, InputType::ShaderResource },
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

            pRootSignature->SetPipelineShaderResourceView(commandList, 0, ShaderResourceView(pCommonVertexBuffer));
            pRootSignature->SetPipelineShaderResourceView(commandList, 1, ShaderResourceView(pCommonIndexBuffer));

            const auto& pMeshletsBuffer = context.m_ResourcePool->GetBuffer(::ResourceIds::User::MeshletsBuffer);

            pRootSignature->SetPipelineShaderResourceView(commandList, 2, ShaderResourceView(pMeshletsBuffer));

            commandList.SetPrimitiveTopology(Mesh::PRIMITIVE_TOPOLOGY);

            for (uint32_t goIndex = 0; auto& go : demo.m_GameObjects)
            {
                const auto& pMaterial = go.GetMaterial();

                pMaterial->Bind(commandList);

                for (uint32_t meshIndex = 0; auto& pMesh : go.GetModel()->GetMeshes())
                {
                    const MeshletBuilder::MeshletSet& meshletSet = demo.m_MeshletSets[goIndex][meshIndex];

                    for (uint32_t meshletIndex = 0; const Meshlet& meshlet : meshletSet.m_Meshlets)
                    {
                        CBuffer::Model modelCBuffer{};
                        modelCBuffer.Compute(go.GetWorldMatrix());
                        modelCBuffer.g_Model_Index = meshletIndex;
                        pRootSignature->SetModelConstantBuffer(commandList, modelCBuffer);

                        commandList.Draw(meshlet.m_TriangleCount);
                        meshletIndex++;
                    }

                    ++meshIndex;
                    break; // TODO: REMOVE
                }

                pMaterial->Unbind(commandList);
                ++goIndex;
                break; // TODO: REMOVE
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
        BufferDescription{ ::ResourceIds::User::CommonVertexBuffer, [](const auto&) { return 10000; }, sizeof(VertexAttributes), CopyDestination },
        BufferDescription{ ::ResourceIds::User::CommonIndexBuffer, [](const auto&) { return 100000 * sizeof(uint16_t); }, 1, CopyDestination },
        BufferDescription{ ::ResourceIds::User::MeshletsBuffer, [](const auto&) { return 1000; }, sizeof(Meshlet), CopyDestination },
    };

    std::vector<TokenDescription> tokens =
    {
        { ::ResourceIds::User::SetupFinishedToken },
    };

    return std::make_unique<RenderGraphRoot>(std::move(renderPasses), std::move(textures), std::move(buffers), std::move(tokens));
}
