#pragma once

#include <DX12Library/Camera.h>
#include <DX12Library/Game.h>
#include <DX12Library/IndexBuffer.h>
#include <DX12Library/Window.h>
#include <DX12Library/RenderTarget.h>
#include <DX12Library/RootSignature.h>
#include <DX12Library/Texture.h>
#include <DX12Library/VertexBuffer.h>
#include <DX12Library/StructuredBuffer.h>

#include <Framework/Light.h>
#include <Framework/GameObject.h>
#include <Framework/GraphicsSettings.h>
#include <Framework/Mesh.h>
#include <Framework/Material.h>
#include <Framework/CommonRootSignature.h>
#include <Framework/Bloom.h>

#include <RenderGraph/RenderGraphRoot.h>

class RenderGraphDemo final : public Game
{
public:
    using Base = Game;

    RenderGraphDemo(const std::wstring& name, int width, int height, GraphicsSettings graphicsSettings);
    ~RenderGraphDemo() override;

    bool LoadContent() override;
    void UnloadContent() override;

protected:
    void OnUpdate(UpdateEventArgs& e) override;
    void OnRender(RenderEventArgs& e) override;
    void OnKeyPressed(KeyEventArgs& e) override;
    void OnKeyReleased(KeyEventArgs& e) override;
    void OnMouseMoved(MouseMotionEventArgs& e) override;
    void OnMouseWheel(MouseWheelEventArgs& e) override;
    void OnResize(ResizeEventArgs& e) override;

private:
    std::shared_ptr<Texture> m_WhiteTexture2d;

    std::unique_ptr<RenderGraph::RenderGraphRoot> m_RenderGraph;

    Camera m_Camera;
    std::vector<GameObject> m_GameObjects;
    DirectionalLight m_DirectionalLight;

    std::shared_ptr<CommonRootSignature> m_RootSignature;

    D3D12_VIEWPORT m_Viewport;
    D3D12_RECT m_ScissorRect;

    GraphicsSettings m_GraphicsSettings;

    double m_Time = 0;

    static constexpr size_t ALIGNMENT = 16;

    struct alignas(ALIGNMENT) CameraData
    {
        DirectX::XMVECTOR m_InitialPosition;
        DirectX::XMVECTOR m_InitialQRotation;
    };

    CameraData* m_PAlignedCameraData;

    struct
    {
        float m_Forward;
        float m_Backward;
        float m_Left;
        float m_Right;
        float m_Up;
        float m_Down;

        float m_Pitch;
        float m_Yaw;

        bool m_Shift;
    } m_CameraController;

    int m_Width;
    int m_Height;
};
