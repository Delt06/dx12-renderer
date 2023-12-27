#include "RenderGraph.User.h"

#include <DirectXMath.h>

#include <DX12Library/StructuredBuffer.h>

#include <Framework/Blit_VS.h>
#include <Framework/Light.h>
#include <Framework/ComputeShader.h>
#include <Framework/Material.h>
#include <Framework/Mesh.h>
#include "Framework/Model.h"
#include <Framework/Shader.h>
#include <Framework/SharedUploadBuffer.h>

#include "MeshletDrawIndirect.h"
#include "MeshletsDemo.h"

using namespace DirectX;

namespace ResourceIds
{
    class User
    {
    public:
        // Textures
        static inline const RenderGraph::ResourceId DepthBuffer = RenderGraph::ResourceIds::GetResourceId(L"DepthBuffer");
        static inline const RenderGraph::ResourceId HierarchicalDepthBuffer = RenderGraph::ResourceIds::GetResourceId(L"HierarchicalDepthBuffer");
        static inline const RenderGraph::ResourceId ImGuiRenderTarget = RenderGraph::ResourceIds::GetResourceId(L"ImGuiRenderTarget");

        // Buffers
        static inline const RenderGraph::ResourceId CommonVertexBuffer = RenderGraph::ResourceIds::GetResourceId(L"CommonVertexBuffer");
        static inline const RenderGraph::ResourceId CommonIndexBuffer = RenderGraph::ResourceIds::GetResourceId(L"CommonIndexBuffer");
        static inline const RenderGraph::ResourceId MeshletsBuffer = RenderGraph::ResourceIds::GetResourceId(L"MeshletsBuffer");
        static inline const RenderGraph::ResourceId TransformsBuffer = RenderGraph::ResourceIds::GetResourceId(L"TransformsBuffer");
        static inline const RenderGraph::ResourceId MeshletDrawCommands = RenderGraph::ResourceIds::GetResourceId(L"MeshletDrawCommands");

        // Tokens
        static inline const RenderGraph::ResourceId SetupFinishedToken = RenderGraph::ResourceIds::GetResourceId(L"SetupFinishedToken");
        static inline const RenderGraph::ResourceId OpaqueFinishedToken = RenderGraph::ResourceIds::GetResourceId(L"OpaqueFinishedToken");
        static inline const RenderGraph::ResourceId ImGuiRenderFinished = RenderGraph::ResourceIds::GetResourceId(L"ImGuiRenderFinished");
        static inline const RenderGraph::ResourceId MainViewFinishedToken = RenderGraph::ResourceIds::GetResourceId(L"MainViewFinishedToken");
    };
}

namespace
{
    constexpr FLOAT CLEAR_COLOR[] = { 53.0f / 255.0f, 82.0f / 255.0f, 138.0f / 255.0f, 1.0f };
    constexpr FLOAT IMGUI_CLEAR_COLOR[] = { 0.0f, 0.0f, 0.0f, 0.0f };
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

        uint32_t g_Pipeline_SelectedMeshletIndex;
        uint32_t g_Pipeline_DebugGpuCulling;
    };

    struct MeshletRootConstants
    {
        uint32_t g_Meshlet_Index;
    };
}

std::unique_ptr<RenderGraph::RenderGraphRoot> RenderGraph::User::Create(
    MeshletsDemo& demo,
    const std::shared_ptr<CommonRootSignature>& pRootSignature,
    CommandList& cmd
)
{
    using namespace RenderGraph;
    using namespace DirectX;

    const auto pSharedUploadBuffer = std::make_shared<SharedUploadBuffer>();
    const auto pMeshletDrawIncorrect = std::make_shared<MeshletDrawIndirect>(pRootSignature, demo.m_MeshletDrawMaterial);

    std::vector<std::unique_ptr<RenderPass>> renderPasses;
    renderPasses.emplace_back(RenderPass::Create(
        L"Setup",
        {
            { ::ResourceIds::User::ImGuiRenderFinished, InputType::Token }
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

            pipelineCBuffer.g_Pipeline_SelectedMeshletIndex = demo.m_SelectedMeshletIndex;
            pipelineCBuffer.g_Pipeline_DebugGpuCulling = demo.m_DebugGpuCulling ? 1 : 0;

            pRootSignature->Bind(commandList);
            pRootSignature->SetPipelineConstantBuffer(commandList, pipelineCBuffer);

            pSharedUploadBuffer->BeginFrame();
        }
    ));

    renderPasses.emplace_back(RenderPass::Create(
        L"Prepare Buffers",
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


    struct OccluderCBuffer
    {
        XMMATRIX m_WorldMatrix;
        XMMATRIX m_InverseTransposeWorldMatrix;

        explicit OccluderCBuffer(const XMMATRIX& worldMatrix)
            : m_WorldMatrix(worldMatrix)
            , m_InverseTransposeWorldMatrix(XMMatrixTranspose(XMMatrixInverse(nullptr, worldMatrix)))
        { }
    };

    renderPasses.emplace_back(RenderPass::Create(
        L"Render HDB: first mip",
        {
            { ::ResourceIds::User::SetupFinishedToken, InputType::Token }
        },
        {
            { ::ResourceIds::User::HierarchicalDepthBuffer, OutputType::DepthWrite },
        },
        [&demo, pRootSignature,
            pOccluderShader=std::make_shared<Shader>(pRootSignature, ShaderBlob(L"Occluder_VS.cso"), ShaderBlob(L"Occluder_DepthOnly_PS.cso"))]
        (const RenderContext& context, CommandList& commandList)
        {
            for (auto& go : demo.m_OccluderGameObjects)
            {
                pOccluderShader->Bind(commandList);

                const auto cbuffer = OccluderCBuffer(go.GetWorldMatrix());
                pRootSignature->SetMaterialConstantBuffer(commandList, sizeof(cbuffer), &cbuffer);

                go.GetModel()->Draw(commandList);
                pOccluderShader->Unbind(commandList);
            }
        }
    ));

    const auto pBlitMesh = Mesh::CreateBlitTriangle(cmd);

    renderPasses.emplace_back(RenderPass::Create(
        L"Render HDB: remaining mips",
        {
            { ::ResourceIds::User::SetupFinishedToken, InputType::Token }
        },
        {
            { ::ResourceIds::User::HierarchicalDepthBuffer, OutputType::DepthWrite },
        },
        [
            pBlitMesh, pRootSignature,
            pDownsampleHDB=std::make_shared<Shader>(pRootSignature,
                ShaderBlob(ShaderBytecode_Blit_VS, sizeof(ShaderBytecode_Blit_VS)), ShaderBlob(L"HDB_Downsample_PS.cso"),
                [](PipelineStateBuilder& psb)
                {
                    auto desc = CD3DX12_DEPTH_STENCIL_DESC(CD3DX12_DEFAULT{});
                    desc.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
                    psb.WithDepthStencil(desc);
                }
            )
        ]
        (const RenderContext& context, CommandList& commandList)
        {
            const auto& pHdb = context.m_ResourcePool->GetTexture(::ResourceIds::User::HierarchicalDepthBuffer);

            const auto resourceDesc = pHdb->GetD3D12ResourceDesc();
            commandList.FlushResourceBarriers();
            pBlitMesh->Bind(commandList);
            pDownsampleHDB->Bind(commandList);
            const auto pD3dCommandList = commandList.GetGraphicsCommandList();

            uint16_t lastMipLevel = resourceDesc.MipLevels - 1;
            for (uint16_t sourceMip = 0; sourceMip < lastMipLevel; ++sourceMip)
            {
                {
                    const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(pHdb->GetD3D12Resource().Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, sourceMip);
                    pD3dCommandList->ResourceBarrier(1, &barrier);
                }

                const auto destinationMip = sourceMip + 1;
                commandList.SetRenderTarget(
                    *context.m_RenderTargetInfo.m_RenderTarget,
                    0, destinationMip
                );
                commandList.SetAutomaticViewportAndScissorRect(*context.m_RenderTargetInfo.m_RenderTarget, destinationMip);

                auto sourceSrvDesc = D3D12_SHADER_RESOURCE_VIEW_DESC(DXGI_FORMAT_R32_FLOAT, D3D12_SRV_DIMENSION_TEXTURE2D, D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING);
                sourceSrvDesc.Texture2D.MipLevels = 1;
                sourceSrvDesc.Texture2D.MostDetailedMip = sourceMip;
                sourceSrvDesc.Texture2D.PlaneSlice = 0;
                sourceSrvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
                pRootSignature->SetMaterialShaderResourceView(commandList, 0, ShaderResourceView(pHdb, sourceMip, 1, sourceSrvDesc));
                pBlitMesh->Draw(commandList);

                {
                    const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(pHdb->GetD3D12Resource().Get(), D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE, sourceMip);
                    pD3dCommandList->ResourceBarrier(1, &barrier);
                }
            }
        }
    ));

    renderPasses.emplace_back(RenderPass::Create(
        L"Prepare Meshlet Culling",
        {

        },
        {
            { ::ResourceIds::User::MeshletDrawCommands, OutputType::CopyDestination }
        },
        [pSharedUploadBuffer]
        (const RenderContext& context, CommandList& commandList)
        {
            const auto& pMeshletDrawCommands = context.m_ResourcePool->GetStructuredBuffer(::ResourceIds::User::MeshletDrawCommands);

            constexpr uint32_t clearValue = 0;
            pSharedUploadBuffer->Upload(commandList, pMeshletDrawCommands->GetCounterBuffer(), &clearValue, sizeof(uint32_t), sizeof(uint32_t));
        }
    ));

    renderPasses.emplace_back(RenderPass::Create(
        L"Cull Meshlets",
        {
            { ::ResourceIds::User::MeshletsBuffer, InputType::ShaderResource },
            { ::ResourceIds::User::TransformsBuffer, InputType::ShaderResource },
            { ::ResourceIds::User::HierarchicalDepthBuffer, InputType::ShaderResource },
        },
        {
            { ::ResourceIds::User::MeshletDrawCommands, OutputType::UnorderedAccess }
        },
        [&demo, pRootSignature,
            pCullingShader = std::make_shared<ComputeShader>(pRootSignature, ShaderBlob(L"MeshletCulling_CS.cso"))]
        (const RenderContext& context, CommandList& commandList)
        {
            const auto& pMeshletsBuffer = context.m_ResourcePool->GetBuffer(::ResourceIds::User::MeshletsBuffer);
            const auto& pTransformsBuffer = context.m_ResourcePool->GetBuffer(::ResourceIds::User::TransformsBuffer);
            const auto& pMeshletDrawCommands = context.m_ResourcePool->GetBuffer(::ResourceIds::User::MeshletDrawCommands);
            const auto& pHdb = context.m_ResourcePool->GetTexture(::ResourceIds::User::HierarchicalDepthBuffer);

            pRootSignature->SetPipelineShaderResourceView(commandList, 2, ShaderResourceView(pMeshletsBuffer));
            pRootSignature->SetPipelineShaderResourceView(commandList, 3, ShaderResourceView(pTransformsBuffer));

            const auto hdbDesc = pHdb->GetD3D12ResourceDesc();
            {

                auto hdbSrcDesc = D3D12_SHADER_RESOURCE_VIEW_DESC(DXGI_FORMAT_R32_FLOAT, D3D12_SRV_DIMENSION_TEXTURE2D, D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING);
                hdbSrcDesc.Texture2D.MipLevels = hdbDesc.MipLevels;
                hdbSrcDesc.Texture2D.MostDetailedMip = 0;
                hdbSrcDesc.Texture2D.PlaneSlice = 0;
                hdbSrcDesc.Texture2D.ResourceMinLODClamp = 0.0f;
                pRootSignature->SetComputeShaderResourceView(commandList, 0, ShaderResourceView(pHdb, 0, -1, hdbSrcDesc));
            }

            pRootSignature->SetUnorderedAccessView(commandList, 0, UnorderedAccessView(pMeshletDrawCommands));

            const uint32_t meshletsCount = demo.m_MeshletsBuffer.m_Meshlets.size();

            {
                struct
                {
                    XMFLOAT3 m_CameraPosition;
                    uint32_t m_TotalCount;
                    Camera::Frustum m_Frustum;
                    XMMATRIX m_ViewProjection;
                    uint32_t m_HDB_Resolution_Width;
                    uint32_t m_HDB_Resolution_Height;
                    uint32_t m_OcclusionCullingMode;
                    uint32_t m_Debug;
                } constants;

                XMStoreFloat3(&constants.m_CameraPosition, demo.m_CullingCameraPosition);
                constants.m_TotalCount = meshletsCount;
                constants.m_Frustum = demo.m_Camera.GetFrustum(demo.m_CullingCameraPosition, demo.m_CullingCameraRotation);
                constants.m_ViewProjection = demo.m_Camera.GetViewMatrix() * demo.m_Camera.GetProjectionMatrix();
                constants.m_HDB_Resolution_Width = hdbDesc.Width;
                constants.m_HDB_Resolution_Height = hdbDesc.Height;
                constants.m_OcclusionCullingMode = demo.m_OcclusionCullingMode;
                constants.m_Debug = demo.m_DebugGpuCulling ? 1 : 0;

                pRootSignature->SetComputeConstantBuffer(commandList, constants);
            }

            constexpr uint32_t threadBlockSize = 32;

            pCullingShader->Bind(commandList);
            commandList.Dispatch(Math::AlignUp(meshletsCount, threadBlockSize) / threadBlockSize, 1, 1);
        }
    ));

    renderPasses.emplace_back(RenderPass::Create(
        L"Draw Meshlets",
        {
            { ::ResourceIds::User::CommonVertexBuffer, InputType::ShaderResource },
            { ::ResourceIds::User::CommonIndexBuffer, InputType::ShaderResource },
            { ::ResourceIds::User::MeshletsBuffer, InputType::ShaderResource },
            { ::ResourceIds::User::TransformsBuffer, InputType::ShaderResource },
            { ::ResourceIds::User::MeshletDrawCommands, InputType::IndirectArgument },
        },
        {
            { ResourceIds::GRAPH_OUTPUT, OutputType::RenderTarget },
            { ::ResourceIds::User::DepthBuffer, OutputType::DepthWrite },
            { ::ResourceIds::User::OpaqueFinishedToken, OutputType::Token }
        },
        [&demo, pRootSignature, pMeshletDrawIncorrect](const RenderContext& context, CommandList& commandList)
        {
            const auto& pCommonVertexBuffer = context.m_ResourcePool->GetBuffer(::ResourceIds::User::CommonVertexBuffer);
            const auto& pCommonIndexBuffer = context.m_ResourcePool->GetBuffer(::ResourceIds::User::CommonIndexBuffer);
            const auto& pMeshletsBuffer = context.m_ResourcePool->GetBuffer(::ResourceIds::User::MeshletsBuffer);
            const auto& pTransformsBuffer = context.m_ResourcePool->GetBuffer(::ResourceIds::User::TransformsBuffer);
            const auto& pMeshletDrawCommands = context.m_ResourcePool->GetStructuredBuffer(::ResourceIds::User::MeshletDrawCommands);

            pRootSignature->SetPipelineShaderResourceView(commandList, 0, ShaderResourceView(pCommonVertexBuffer));
            pRootSignature->SetPipelineShaderResourceView(commandList, 1, ShaderResourceView(pCommonIndexBuffer));
            pRootSignature->SetPipelineShaderResourceView(commandList, 2, ShaderResourceView(pMeshletsBuffer));
            pRootSignature->SetPipelineShaderResourceView(commandList, 3, ShaderResourceView(pTransformsBuffer));

            pMeshletDrawIncorrect->DrawIndirect(commandList,
                demo.m_MeshletsBuffer.m_Meshlets.size(),
                *pMeshletDrawCommands);
        }
    ));

    renderPasses.emplace_back(RenderPass::Create(
        L"Draw Occluders",
        {
            { ::ResourceIds::User::OpaqueFinishedToken, InputType::Token },
        },
        {
            { ResourceIds::GRAPH_OUTPUT, OutputType::RenderTarget },
            { ::ResourceIds::User::DepthBuffer, OutputType::DepthRead }
        },
        [&demo, pRootSignature](const RenderContext&, CommandList& commandList)
        {
            if (!demo.m_RenderOccluders)
            {
                return;
            }

            for (auto& go : demo.m_OccluderGameObjects)
            {
                const auto& pMaterial = go.GetMaterial();
                pMaterial->Bind(commandList);

                const auto cbuffer = OccluderCBuffer(go.GetWorldMatrix());
                pRootSignature->SetMaterialConstantBuffer(commandList, sizeof(cbuffer), &cbuffer);

                go.GetModel()->Draw(commandList);
                pMaterial->Unbind(commandList);
            }
        }
    ));

    const auto debugGeometryPsb = [](PipelineStateBuilder& psb)
    {
        auto desc = CD3DX12_RASTERIZER_DESC(CD3DX12_DEFAULT{});
        desc.FillMode = D3D12_FILL_MODE_WIREFRAME;
        desc.CullMode = D3D12_CULL_MODE_NONE;
        psb.WithRasterizer(desc);
        psb.WithDisabledDepthWrite();
    };

    renderPasses.emplace_back(RenderPass::Create(
        L"Debug: Meshlet Bounds",
        {
            { ::ResourceIds::User::OpaqueFinishedToken, InputType::Token },
        },
        {
            { ResourceIds::GRAPH_OUTPUT, OutputType::RenderTarget },
            { ::ResourceIds::User::DepthBuffer, OutputType::DepthRead },
            { ::ResourceIds::User::MainViewFinishedToken, OutputType::Token }
        },
        [
            pBoundingSphereMesh = Mesh::CreateSphere(cmd, 2, 8),
            pBoundingSphereShader = std::make_shared<Shader>(pRootSignature, ShaderBlob(L"DebugBoundingSphere_VS.cso"), ShaderBlob(L"SelectedMeshletColor_PS.cso"), debugGeometryPsb),
            pBoundingSquareMesh = Mesh::CreateVerticalQuad(cmd, 2, 2),
            pBoundingSquareShader = std::make_shared<Shader>(pRootSignature, ShaderBlob(L"DebugBoundingSquare_VS.cso"), ShaderBlob(L"SelectedMeshletColor_PS.cso"), debugGeometryPsb),
            pAabbMesh = Mesh::CreateCube(cmd, 2),
            pAabbShader = std::make_shared<Shader>(pRootSignature, ShaderBlob(L"DebugAABB_VS.cso"), ShaderBlob(L"SelectedMeshletColor_PS.cso"), debugGeometryPsb)
        ](const RenderContext&, CommandList& commandList)
        {
            {
                pBoundingSphereShader->Bind(commandList);
                pBoundingSphereMesh->Draw(commandList, 1);
            }

            {
                pBoundingSquareShader->Bind(commandList);
                pBoundingSquareMesh->Draw(commandList, 1);
            }

            {
                pAabbShader->Bind(commandList);
                pAabbMesh->Draw(commandList, 1);
            }
        }
    ));

    renderPasses.emplace_back(
        RenderPass::Create(
            L"ImGui: Render",
            {
            },
            {
                { ::ResourceIds::User::ImGuiRenderTarget, OutputType::RenderTarget },
                { ::ResourceIds::User::ImGuiRenderFinished, OutputType::Token },
            },
            [&demo](const RenderContext&, CommandList& commandList)
            {
                demo.m_ImGui->DrawToRenderTarget(commandList);
            }
        ));

    renderPasses.emplace_back(
        RenderPass::Create(
            L"ImGui: Copy to Output",
            {
                { ::ResourceIds::User::ImGuiRenderTarget, InputType::ShaderResource },
                { ::ResourceIds::User::MainViewFinishedToken, InputType::Token },
            },
            {
                { ResourceIds::GRAPH_OUTPUT, OutputType::RenderTarget },
            },
            [&demo](const RenderContext& context, CommandList& commandList)
            {
                const auto& pImGuiRenderTarget = context.m_ResourcePool->GetTexture(::ResourceIds::User::ImGuiRenderTarget);
                demo.m_ImGui->BlitCombine(commandList, pImGuiRenderTarget);
            }
        ));

    const RenderMetadataExpression renderWidthExpression = [](const RenderMetadata& metadata) { return metadata.m_ScreenWidth; };
    const RenderMetadataExpression renderHeightExpression = [](const RenderMetadata& metadata) { return metadata.m_ScreenHeight; };
    const RenderMetadataExpression hdbSizeExpression = [&demo](const RenderMetadata&) { return demo.m_HdbResolution; };


    auto hdbDesc = TextureDescription{ ::ResourceIds::User::HierarchicalDepthBuffer, hdbSizeExpression, hdbSizeExpression, DEPTH_BUFFER_FORMAT, { 1.0f, 0u }, Clear };
    hdbDesc.m_MipLevels = 0;

    std::vector textures = {
        TextureDescription{ ResourceIds::GRAPH_OUTPUT, renderWidthExpression, renderHeightExpression, Window::BUFFER_FORMAT_SRGB, CLEAR_COLOR, Clear },
        TextureDescription{ ::ResourceIds::User::DepthBuffer, renderWidthExpression, renderHeightExpression, DEPTH_BUFFER_FORMAT, { 1.0f, 0u }, Clear },
        TextureDescription{ ::ResourceIds::User::ImGuiRenderTarget, renderWidthExpression, renderHeightExpression, ImGuiImpl::BUFFER_FORMAT, IMGUI_CLEAR_COLOR, Clear },
        hdbDesc
    };

    std::vector buffers =
    {
        BufferDescription{ ::ResourceIds::User::CommonVertexBuffer, [&demo](const auto&) { return demo.m_MeshletsBuffer.m_MeshPrototype.m_Vertices.size(); }, sizeof(VertexAttributes), CopyDestination },
        BufferDescription{ ::ResourceIds::User::CommonIndexBuffer, [&demo](const auto&) { return demo.m_MeshletsBuffer.m_MeshPrototype.m_Indices.size() * sizeof(uint16_t); }, 1, CopyDestination },
        BufferDescription{ ::ResourceIds::User::MeshletsBuffer, [&demo](const auto&) { return demo.m_MeshletsBuffer.m_Meshlets.size(); }, sizeof(Meshlet), CopyDestination },
        BufferDescription{ ::ResourceIds::User::TransformsBuffer, [&demo](const auto&) { return demo.m_TransformsBuffer.size(); }, sizeof(Transform), CopyDestination },
        BufferDescription{ ::ResourceIds::User::MeshletDrawCommands, [&demo](const auto&) { return demo.m_MeshletsBuffer.m_Meshlets.size(); }, sizeof(MeshletDrawIndirectCommand), CopyDestination },
    };

    std::vector<TokenDescription> tokens =
    {
        { ::ResourceIds::User::SetupFinishedToken },
        { ::ResourceIds::User::OpaqueFinishedToken },
        { ::ResourceIds::User::ImGuiRenderFinished },
        { ::ResourceIds::User::MainViewFinishedToken },
    };

    return std::make_unique<RenderGraphRoot>(std::move(renderPasses), std::move(textures), std::move(buffers), std::move(tokens));
}
