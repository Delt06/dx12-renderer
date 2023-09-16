#include "RenderGraph.User.h"

#include <DirectXMath.h>

#include <DX12Library/StructuredBuffer.h>

#include <Framework/Blit_VS.h>
#include <Framework/Light.h>
#include <Framework/ComputeShader.h>
#include <Framework/Material.h>
#include <Framework/Mesh.h>
#include <Framework/Shader.h>

#include "MeshletsDemo.h"

using namespace DirectX;

namespace ResourceIds
{
    class User
    {
    public:
        static inline const RenderGraph::ResourceId DepthBuffer = RenderGraph::ResourceIds::GetResourceId(L"DepthBuffer");

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

    std::vector<std::unique_ptr<RenderPass>> renderPasses;
    renderPasses.emplace_back(RenderPass::Create(
        L"Setup",
        {

        },
        {
            { ::ResourceIds::User::SetupFinishedToken, OutputType::Token }
        },
        [&demo, pRootSignature](const RenderContext& context, CommandList& commandList)
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
        }
    ));

    renderPasses.emplace_back(RenderPass::Create(
        L"Draw Geometries",
        {
            { ::ResourceIds::User::SetupFinishedToken, InputType::Token }
        },
        {
            { ResourceIds::GraphOutput, OutputType::RenderTarget },
            { ::ResourceIds::User::DepthBuffer, OutputType::DepthWrite }
        },
        [&demo, pRootSignature](const RenderContext& context, CommandList& commandList)
        {
            for (const auto& go : demo.m_GameObjects)
            {
                CBuffer::Model modelCBuffer{};
                modelCBuffer.Compute(go.GetWorldMatrix());
                pRootSignature->SetModelConstantBuffer(commandList, modelCBuffer);

                go.Draw(commandList);
            }
        }
    ));

    RenderMetadataExpression renderWidthExpression = [](const RenderMetadata& metadata) { return metadata.m_ScreenWidth; };
    RenderMetadataExpression renderHeightExpression = [](const RenderMetadata& metadata) { return metadata.m_ScreenHeight; };


    std::vector<TextureDescription> textures = {
        { ::ResourceIds::User::DepthBuffer, renderWidthExpression, renderHeightExpression, DEPTH_BUFFER_FORMAT, { 1.0f, 0u }, Clear },
        { ResourceIds::GraphOutput, renderWidthExpression, renderHeightExpression, Window::BUFFER_FORMAT_SRGB, CLEAR_COLOR, Clear },
    };

    std::vector<BufferDescription> buffers =
    {
        //BufferDescription( ::ResourceIds::User::ColorSplitBuffer, [](const RenderMetadata& metadata) { return 32LU; }, sizeof(ColorSplitEntry), ResourceInitAction::CopyDestination ),
    };

    std::vector<TokenDescription> tokens =
    {
        { ::ResourceIds::User::SetupFinishedToken },
    };

    return std::make_unique<RenderGraphRoot>(std::move(renderPasses), std::move(textures), std::move(buffers), std::move(tokens));
}
