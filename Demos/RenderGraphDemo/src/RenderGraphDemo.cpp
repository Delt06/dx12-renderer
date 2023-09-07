#include "RenderGraphDemo.h"

#include <DX12Library/Application.h>
#include <DX12Library/CommandQueue.h>
#include <DX12Library/CommandList.h>
#include <DX12Library/Helpers.h>
#include <DX12Library/Window.h>
#include <Framework/Bone.h>
#include <Framework/Animation.h>
#include <Framework/GameObject.h>
#include <Framework/Light.h>

#include <wrl.h>

#include <Framework/ComputeShader.h>
#include <Framework/GraphicsSettings.h>
#include <Framework/Model.h>
#include <Framework/ModelLoader.h>
#include <Framework/Blit_VS.h>

using namespace Microsoft::WRL;

#include <d3d12.h>
#include <d3dx12.h>
#include <DirectXColors.h>

using namespace DirectX;

#include <algorithm> // For std::min and std::max.
#if defined(min)
#undef min
#endif

#include <DX12Library/ShaderUtils.h>

#if defined(max)
#undef max
#endif

#include "ResourceIds.User.h"

namespace
{

    // Clamp a value between a min and max range.
    template <typename T>
    constexpr const T& Clamp(const T& val, const T& min, const T& max)
    {
        return val < min ? min : val > max ? max : val;
    }

    template<typename T>
    constexpr const T& Remap(const T& low1, const T& high1, const T& low2, const T& high2, const T& value)
    {
        return low2 + (value - low1) * (high2 - low2) / (high1 - low1);
    }

    template<typename T>
    constexpr T Smoothstep(const T& edge0, const T& edge1, const T& x)
    {
        auto t = Clamp<T>((x - edge0) / (edge1 - edge0), 0, 1);
        return t * t * (3 - 2 * t);
    }

    bool allowFullscreenToggle = true;
    constexpr FLOAT CLEAR_COLOR[] = { 138.0f / 255.0f, 82.0f / 255.0f, 52.0f / 255.0f, 1.0f };
    constexpr FLOAT ZERO_CLEAR_COLOR[] = { 0.5f, 0.5f, 0.5f, 1.0f };

    // Builds a look-at (world) matrix from a point, up and direction vectors.
    XMMATRIX XM_CALLCONV LookAtMatrix(FXMVECTOR position, FXMVECTOR direction, FXMVECTOR up)
    {
        assert(!XMVector3Equal(direction, XMVectorZero()));
        assert(!XMVector3IsInfinite(direction));
        assert(!XMVector3Equal(up, XMVectorZero()));
        assert(!XMVector3IsInfinite(up));

        XMVECTOR R2 = XMVector3Normalize(direction);

        XMVECTOR R0 = XMVector3Cross(up, R2);
        R0 = XMVector3Normalize(R0);

        XMVECTOR R1 = XMVector3Cross(R2, R0);

        XMMATRIX M(R0, R1, R2, position);

        return M;
    }

    UINT GetMsaaQualityLevels(ComPtr<ID3D12Device2> device, DXGI_FORMAT format, UINT sampleCount)
    {
        D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msLevels;
        msLevels.Format = format;
        msLevels.SampleCount = sampleCount;
        msLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;

        ThrowIfFailed(device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &msLevels, sizeof(msLevels)));
        return msLevels.NumQualityLevels;
    }
}

namespace Pipeline
{
    constexpr UINT SHADOW_MAP_REGISTER_INDEX = 0;
    constexpr UINT SSAO_MAP_REGISTER_INDEX = 1;
}

namespace
{
    std::wstring GetClassName(const char* fullFuncName)
    {
        std::wstring fullFuncNameStr(fullFuncName, fullFuncName + strlen(fullFuncName));
        size_t pos = fullFuncNameStr.find_last_of(L"::");
        if (pos == std::string::npos)
        {
            return L"";
        }
        return fullFuncNameStr.substr(0, pos-1);
    }

#define __CLASS_NAME__ GetClassName(__FUNCTION__)

    class ColorSplitBufferComputePass : public RenderGraph::RenderPass
    {
    public:
        ColorSplitBufferComputePass(const std::shared_ptr<CommonRootSignature>& pRootSignature) : m_RootSignature(pRootSignature) {}

    protected:
        virtual void InitImpl(CommandList& commandList) override
        {
            SetPassName(__CLASS_NAME__);

            RegisterInput({ ResourceIds::User::SetupFinishedToken, InputType::Token });

            RegisterOutput({ ResourceIds::User::ColorSplitBuffer, OutputType::UnorderedAccess });

            m_ComputeShader = std::make_shared<ComputeShader>(m_RootSignature,
                ShaderBlob(L"ColorSplitBuffer_CS.cso")
            );
        }

        virtual void ExecuteImpl(const RenderGraph::RenderContext& context, CommandList& commandList) override
        {
            m_ComputeShader->Bind(commandList);

            const auto& pBuffer = context.m_ResourcePool->GetBuffer(ResourceIds::User::ColorSplitBuffer);
            m_RootSignature->SetUnorderedAccessView(commandList, 0,
                UnorderedAccessView(pBuffer)
            );
            m_RootSignature->SetComputeConstantBuffer<uint64_t>(commandList, context.m_Metadata.m_FrameIndex);

            commandList.Dispatch(1, 1, 1);
        }

    private:
        std::shared_ptr<CommonRootSignature> m_RootSignature;
        std::shared_ptr<ComputeShader> m_ComputeShader;
    };

    class ColorSplitBufferCountResetPass : public RenderGraph::RenderPass
    {
    protected:
        virtual void InitImpl(CommandList& commandList) override
        {
            SetPassName(__CLASS_NAME__);

            RegisterInput({ ResourceIds::User::SetupFinishedToken, InputType::Token });

            RegisterOutput({ ResourceIds::User::ColorSplitBuffer, OutputType::CopyDestination });
        }

        virtual void ExecuteImpl(const RenderGraph::RenderContext& context, CommandList& commandList) override
        {
            const auto& pBuffer = context.m_ResourcePool->GetBuffer(ResourceIds::User::ColorSplitBuffer);
            commandList.CopyByteAddressBuffer<uint32_t>(pBuffer->GetCounterBuffer(), 0u);
        }
    };

    class PostProcessingRenderPass : public RenderGraph::RenderPass
    {
    public:
        PostProcessingRenderPass(const std::shared_ptr<CommonRootSignature>& pRootSignature) : m_RootSignature(pRootSignature) {}

    protected:
        virtual void InitImpl(CommandList& commandList) override
        {
            SetPassName(__CLASS_NAME__);

            RegisterInput({ ResourceIds::User::TempRenderTarget, InputType::ShaderResource });
            RegisterInput({ ResourceIds::User::ColorSplitBuffer, InputType::ShaderResource });

            RegisterOutput({ RenderGraph::ResourceIds::GraphOutput, OutputType::RenderTarget });

            m_BlitMesh = Mesh::CreateBlitTriangle(commandList);
            auto pShader = std::make_shared<Shader>(m_RootSignature,
                ShaderBlob(ShaderBytecode_Blit_VS, sizeof(ShaderBytecode_Blit_VS)),
                ShaderBlob(L"PostProcessing_PS.cso")
            );
            m_Material = std::make_shared<Material>(pShader);

        }

        virtual void ExecuteImpl(const RenderGraph::RenderContext& context, CommandList& commandList) override
        {
            const auto& pTempRt = context.m_ResourcePool->GetTexture(ResourceIds::User::TempRenderTarget);
            const auto& pColorSplitBuffer = context.m_ResourcePool->GetBuffer(ResourceIds::User::ColorSplitBuffer);
            const auto& ppColorSplitBufferCounter = pColorSplitBuffer->GetCounterBufferPtr();

            m_Material->SetShaderResourceView("_Source", ShaderResourceView(pTempRt));
            m_Material->SetShaderResourceView("_ColorSplitBuffer", ShaderResourceView(pColorSplitBuffer));
            m_Material->SetShaderResourceView("_ColorSplitBufferCounter", ShaderResourceView(ppColorSplitBufferCounter));

            auto desc = pTempRt->GetD3D12ResourceDesc();
            m_Material->SetVariable("_TexelSize", XMFLOAT2{ 1.0f / desc.Width, 1.0f / desc.Height });
            m_Material->SetVariable("_Time", static_cast<float>(context.m_Metadata.m_Time));
            m_Material->Bind(commandList);

            m_BlitMesh->Draw(commandList);
        }

    private:
        std::shared_ptr<Mesh> m_BlitMesh;
        std::shared_ptr<Material> m_Material;
        std::shared_ptr<CommonRootSignature> m_RootSignature;
    };

    class SetupRenderPass : public RenderGraph::RenderPass
    {
    public:
        SetupRenderPass(const std::shared_ptr<CommonRootSignature>& pRootSignature) : m_RootSignature(pRootSignature) {}
    protected:
        virtual void InitImpl(CommandList& commandList) override
        {
            SetPassName(__CLASS_NAME__);

            RegisterOutput({ ResourceIds::User::TempRenderTarget3, OutputType::RenderTarget });
            RegisterOutput({ ResourceIds::User::SetupFinishedToken, OutputType::Token });
        }

        virtual void ExecuteImpl(const RenderGraph::RenderContext& context, CommandList& commandList) override
        {
            m_RootSignature->Bind(commandList);
        }

    private:
        std::shared_ptr<CommonRootSignature> m_RootSignature;
    };

    class CopyTempRtRenderPass : public RenderGraph::RenderPass
    {
    protected:
        virtual void InitImpl(CommandList& commandList) override
        {
            SetPassName(__CLASS_NAME__);

            RegisterInput({ ResourceIds::User::TempRenderTarget3, InputType::CopySource });

            RegisterOutput({ ResourceIds::User::TempRenderTarget, OutputType::CopyDestination });
            RegisterOutput({ ResourceIds::User::TempRenderTargetReadyToken, OutputType::Token });
        }

        virtual void ExecuteImpl(const RenderGraph::RenderContext& context, CommandList& commandList) override
        {
            const auto& src = context.m_ResourcePool->GetResource(ResourceIds::User::TempRenderTarget3);
            const auto& dst = context.m_ResourcePool->GetResource(ResourceIds::User::TempRenderTarget);
            commandList.CopyResource(dst, src);
        }
    };

    class DrawQuadRenderPass : public RenderGraph::RenderPass
    {
    public:
        DrawQuadRenderPass(const std::shared_ptr<CommonRootSignature>& pRootSignature) : m_RootSignature(pRootSignature) {}

    protected:
        virtual void InitImpl(CommandList& commandList) override
        {
            SetPassName(__CLASS_NAME__);

            RegisterInput({ ResourceIds::User::TempRenderTargetReadyToken, InputType::Token });

            RegisterOutput({ ResourceIds::User::TempRenderTarget, OutputType::RenderTarget });

            m_Mesh = Mesh::CreateVerticalQuad(commandList, 0.5f, 0.5f);
            m_Shader = std::make_shared<Shader>(m_RootSignature,
                ShaderBlob(ShaderBytecode_Blit_VS, sizeof(ShaderBytecode_Blit_VS)),
                ShaderBlob(L"WhiteShader_PS.cso")
            );
        }

        virtual void ExecuteImpl(const RenderGraph::RenderContext& context, CommandList& commandList) override
        {
            m_Shader->Bind(commandList);

            m_Mesh->Bind(commandList);
            m_Mesh->Draw(commandList);
        }

    private:
        std::shared_ptr<Mesh> m_Mesh;
        std::shared_ptr<Shader> m_Shader;
        std::shared_ptr<CommonRootSignature> m_RootSignature;
    };

    class UselessPassRenderPass : public RenderGraph::RenderPass
    {
    protected:
        virtual void InitImpl(CommandList& commandList) override
        {
            SetPassName(__CLASS_NAME__);

            RegisterOutput({ ResourceIds::User::TempRenderTarget2, OutputType::RenderTarget });
        }

        virtual void ExecuteImpl(const RenderGraph::RenderContext& context, CommandList& commandList) override
        {}
    };
}

RenderGraphDemo::RenderGraphDemo(const std::wstring& name, int width, int height, GraphicsSettings graphicsSettings)
    : Base(name, width, height, graphicsSettings.VSync)
    , m_Viewport(CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)))
    , m_ScissorRect(CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX))
    , m_GraphicsSettings(graphicsSettings)
    , m_CameraController{}
    , m_Width(0)
    , m_Height(0)
{
    const XMVECTOR cameraPos = XMVectorSet(0, 5, -20, 1);
    const XMVECTOR cameraTarget = XMVectorSet(0, 5, 0, 1);
    const XMVECTOR cameraUp = XMVectorSet(0, 1, 0, 0);

    m_Camera.SetLookAt(cameraPos, cameraTarget, cameraUp);

    m_PAlignedCameraData = static_cast<CameraData*>(_aligned_malloc(sizeof(CameraData), 16));

    m_PAlignedCameraData->m_InitialPosition = m_Camera.GetTranslation();
    m_PAlignedCameraData->m_InitialQRotation = m_Camera.GetRotation();
}

RenderGraphDemo::~RenderGraphDemo()
{
    _aligned_free(m_PAlignedCameraData);
}

bool RenderGraphDemo::LoadContent()
{
    const auto device = Application::Get().GetDevice();
    const auto commandQueue = Application::Get().GetCommandQueue();
    const auto commandList = commandQueue->GetCommandList();

    // sRGB formats provide free gamma correction!
    constexpr DXGI_FORMAT backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    constexpr DXGI_FORMAT depthBufferFormat = DXGI_FORMAT_D32_FLOAT;
    constexpr DXGI_FORMAT normalsBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    constexpr DXGI_FORMAT resolvedDepthBufferFormat = DXGI_FORMAT_R32_FLOAT;
    D3D12_CLEAR_VALUE colorClearValue;
    colorClearValue.Format = backBufferFormat;
    memcpy(colorClearValue.Color, CLEAR_COLOR, sizeof CLEAR_COLOR);

    D3D12_CLEAR_VALUE normalsClearValue;
    normalsClearValue.Format = normalsBufferFormat;
    memcpy(normalsClearValue.Color, ZERO_CLEAR_COLOR, sizeof ZERO_CLEAR_COLOR);

    // Generate default texture
    {
        m_WhiteTexture2d = std::make_shared<Texture>();
        commandList->LoadTextureFromFile(*m_WhiteTexture2d, L"Assets/Textures/white.png");
    }

    m_RootSignature = std::make_shared<CommonRootSignature>(m_WhiteTexture2d);

    m_DirectionalLight.m_Color = XMFLOAT4(1, 1, 1, 1);
    m_DirectionalLight.m_DirectionWs = XMFLOAT4(1, 1, 0, 0);

    {
        using namespace RenderGraph;

        std::vector<std::unique_ptr<RenderPass>> renderPasses;
        renderPasses.emplace_back(std::make_unique<SetupRenderPass>(m_RootSignature));
        renderPasses.emplace_back(std::make_unique<ColorSplitBufferCountResetPass>());
        renderPasses.emplace_back(std::make_unique<ColorSplitBufferComputePass>(m_RootSignature));
        renderPasses.emplace_back(std::make_unique<CopyTempRtRenderPass>());
        renderPasses.emplace_back(std::make_unique<DrawQuadRenderPass>(m_RootSignature));
        renderPasses.emplace_back(std::make_unique<PostProcessingRenderPass>(m_RootSignature));
        renderPasses.emplace_back(std::make_unique<UselessPassRenderPass>());

        RenderMetadataExpression<uint32_t> renderWidthExpression = [](const RenderMetadata& metadata) { return metadata.m_ScreenWidth; };
        RenderMetadataExpression<uint32_t> renderHeightExpression = [](const RenderMetadata& metadata) { return metadata.m_ScreenHeight; };

        std::vector<TextureDescription> textures = {
            {::ResourceIds::User::TempRenderTarget, renderWidthExpression, renderHeightExpression, backBufferFormat, CLEAR_COLOR, ResourceInitAction::CopyDestination},
            {::ResourceIds::User::TempRenderTarget2, renderWidthExpression, renderHeightExpression, backBufferFormat, CLEAR_COLOR, ResourceInitAction::Clear},
            {::ResourceIds::User::TempRenderTarget3, renderWidthExpression, renderHeightExpression, backBufferFormat, CLEAR_COLOR, ResourceInitAction::Clear},
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
            {::ResourceIds::User::TempRenderTargetReadyToken},
        };

        m_RenderGraph = std::make_unique<RenderGraphRoot>(std::move(renderPasses), std::move(textures), std::move(buffers), std::move(tokens));
    }

    auto fenceValue = commandQueue->ExecuteCommandList(commandList);
    commandQueue->WaitForFenceValue(fenceValue);

    return true;
}

void RenderGraphDemo::OnResize(ResizeEventArgs& e)
{
    Base::OnResize(e);

    if (m_Width != e.Width || m_Height != e.Height)
    {
        m_Width = std::max(1, e.Width);
        m_Height = std::max(1, e.Height);

        const float aspectRatio = static_cast<float>(m_Width) / static_cast<float>(m_Height);
        m_Camera.SetProjection(45.0f, aspectRatio, 0.1f, 1000.0f);

        m_Viewport = CD3DX12_VIEWPORT(0.0f, 0.0f,
            static_cast<float>(m_Width), static_cast<float>(m_Height));

        if (m_RenderGraph)
            m_RenderGraph->MarkDirty();
    }
}

void RenderGraphDemo::UnloadContent()
{}

void RenderGraphDemo::OnUpdate(UpdateEventArgs& e)
{
    static uint64_t frameCount = 0;
    static double totalTime = 0.0;

    Base::OnUpdate(e);

    if (e.ElapsedTime > 1.0f) return;

    totalTime += e.ElapsedTime;
    m_Time += e.ElapsedTime;
    frameCount++;

    if (totalTime > 1.0)
    {
        double fps = frameCount / totalTime;

        char buffer[512];
        sprintf_s(buffer, "FPS: %f\n", fps);
        OutputDebugStringA(buffer);

        frameCount = 0;
        totalTime = 0.0;
    }

    // Update the camera.
    float speedMultiplier = (m_CameraController.m_Shift ? 16.0f : 4.0f);

    XMVECTOR cameraTranslate = XMVectorSet(m_CameraController.m_Right - m_CameraController.m_Left, 0.0f,
        m_CameraController.m_Forward - m_CameraController.m_Backward,
        1.0f) * speedMultiplier
        * static_cast<float>(e.ElapsedTime);
    XMVECTOR cameraPan = XMVectorSet(0.0f, m_CameraController.m_Up - m_CameraController.m_Down, 0.0f, 1.0f) *
        speedMultiplier *
        static_cast<float>(e.ElapsedTime);
    m_Camera.Translate(cameraTranslate, Space::Local);
    m_Camera.Translate(cameraPan, Space::Local);

    XMVECTOR cameraRotation = XMQuaternionRotationRollPitchYaw(XMConvertToRadians(m_CameraController.m_Pitch),
        XMConvertToRadians(m_CameraController.m_Yaw), 0.0f);
    m_Camera.SetRotation(cameraRotation);

    auto lightDirectionWS = DirectX::XMLoadFloat4(&m_DirectionalLight.m_DirectionWs);
    lightDirectionWS = DirectX::XMVector3Normalize(lightDirectionWS);
    DirectX::XMStoreFloat4(&m_DirectionalLight.m_DirectionWs, lightDirectionWS);

    auto dt = static_cast<float>(e.ElapsedTime);
}

void RenderGraphDemo::OnRender(RenderEventArgs& e)
{
    Base::OnRender(e);

    static uint64_t frameIndex = 0;

    const XMMATRIX viewMatrix = m_Camera.GetViewMatrix();
    const XMMATRIX projectionMatrix = m_Camera.GetProjectionMatrix();
    const XMMATRIX viewProjectionMatrix = viewMatrix * projectionMatrix;

    {
        RenderGraph::RenderMetadata metadata;
        metadata.m_ScreenWidth = m_Width;
        metadata.m_ScreenHeight = m_Height;
        metadata.m_Time = m_Time;
        metadata.m_FrameIndex = frameIndex;
        m_RenderGraph->Execute(metadata);
    }


    m_RenderGraph->Present(PWindow);
    frameIndex++;
}

void RenderGraphDemo::OnKeyPressed(KeyEventArgs& e)
{
    Base::OnKeyPressed(e);

    switch (e.Key)
    {
    case KeyCode::Escape:
        Application::Get().Quit(0);
        break;
    case KeyCode::Enter:
        if (e.Alt)
        {
    case KeyCode::F11:
        if (allowFullscreenToggle)
        {
            PWindow->ToggleFullscreen();
            allowFullscreenToggle = false;
        }
        break;
        }
    case KeyCode::V:
        PWindow->ToggleVSync();
        m_GraphicsSettings.VSync = !m_GraphicsSettings.VSync;
        break;
    case KeyCode::R:
        // Reset camera transform
        m_Camera.SetTranslation(m_PAlignedCameraData->m_InitialPosition);
        m_Camera.SetRotation(m_PAlignedCameraData->m_InitialQRotation);
        m_CameraController.m_Pitch = 0.0f;
        m_CameraController.m_Yaw = 0.0f;
        break;
    case KeyCode::Up:
    case KeyCode::W:
        m_CameraController.m_Forward = 1.0f;
        break;
    case KeyCode::Left:
    case KeyCode::A:
        m_CameraController.m_Left = 1.0f;
        break;
    case KeyCode::Down:
    case KeyCode::S:
        m_CameraController.m_Backward = 1.0f;
        break;
    case KeyCode::Right:
    case KeyCode::D:
        m_CameraController.m_Right = 1.0f;
        break;
    case KeyCode::Q:
        m_CameraController.m_Down = 1.0f;
        break;
    case KeyCode::E:
        m_CameraController.m_Up = 1.0f;
        break;
    case KeyCode::ShiftKey:
        m_CameraController.m_Shift = true;
        break;
    }
}

void RenderGraphDemo::OnKeyReleased(KeyEventArgs& e)
{
    Base::OnKeyReleased(e);

    switch (e.Key)
    {
    case KeyCode::Enter:
        if (e.Alt)
        {
    case KeyCode::F11:
        allowFullscreenToggle = true;
        }
        break;
    case KeyCode::Up:
    case KeyCode::W:
        m_CameraController.m_Forward = 0.0f;
        break;
    case KeyCode::Left:
    case KeyCode::A:
        m_CameraController.m_Left = 0.0f;
        break;
    case KeyCode::Down:
    case KeyCode::S:
        m_CameraController.m_Backward = 0.0f;
        break;
    case KeyCode::Right:
    case KeyCode::D:
        m_CameraController.m_Right = 0.0f;
        break;
    case KeyCode::Q:
        m_CameraController.m_Down = 0.0f;
        break;
    case KeyCode::E:
        m_CameraController.m_Up = 0.0f;
        break;
    case KeyCode::ShiftKey:
        m_CameraController.m_Shift = false;
        break;
    }
}

void RenderGraphDemo::OnMouseMoved(MouseMotionEventArgs& e)
{
    Base::OnMouseMoved(e);

    const float mouseSpeed = 0.1f;

    if (e.LeftButton)
    {
        m_CameraController.m_Pitch -= e.RelY * mouseSpeed;

        m_CameraController.m_Pitch = Clamp(m_CameraController.m_Pitch, -90.0f, 90.0f);

        m_CameraController.m_Yaw -= e.RelX * mouseSpeed;
    }
}


void RenderGraphDemo::OnMouseWheel(MouseWheelEventArgs& e)
{
    {
        auto fov = m_Camera.GetFov();

        fov -= e.WheelDelta;
        fov = Clamp(fov, 12.0f, 90.0f);

        m_Camera.SetFov(fov);

        char buffer[256];
        sprintf_s(buffer, "FoV: %f\n", fov);
        OutputDebugStringA(buffer);
    }
}
