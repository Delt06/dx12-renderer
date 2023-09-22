#pragma once

#include <memory>

#include <imgui.h>

#include <DX12Library/CommandList.h>
#include <DX12Library/Texture.h>
#include <DX12Library/Window.h>

#include <Framework/CommonRootSignature.h>
#include <Framework/Mesh.h>
#include <Framework/Shader.h>

class ImGuiImpl final
{
public:
    constexpr static DXGI_FORMAT BUFFER_FORMAT = Window::BUFFER_FORMAT;

    ImGuiImpl(CommandList& commandList, const Window& window, const std::shared_ptr<CommonRootSignature>& pRootSignature);
    ~ImGuiImpl();

    bool WantsToCaptureMouse() const;
    bool WantsToCaptureKeyboard() const;

    void BeginFrame() const;
    void Render() const;
    void DrawToRenderTarget(CommandList& commandList);
    void BlitCombine(CommandList& commandList, const std::shared_ptr<Texture>& pSourceTexture) const;

private:
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_SrvDescHeap;
    std::shared_ptr<Shader> m_CombineShader;
    std::shared_ptr<Mesh> m_BlitMesh;
    std::shared_ptr<CommonRootSignature> m_RootSignature;
};
