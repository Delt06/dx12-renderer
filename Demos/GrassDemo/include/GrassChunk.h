#pragma once

#include <memory>

#include <DX12Library/Camera.h>
#include <DX12Library/CommandList.h>
#include <DX12Library/StructuredBuffer.h>
#include <DX12Library/Window.h>

#include <Framework/CommonRootSignature.h>
#include <Framework/ComputeShader.h>
#include <Framework/Mesh.h>

class GrassChunk
{
public:
    explicit GrassChunk(
        const std::shared_ptr<CommonRootSignature>& rootSignature,
        const Microsoft::WRL::ComPtr<ID3D12CommandSignature>& indirectCommandSignature,
        const std::shared_ptr<Mesh>& mesh,
        CommandList& commandList,
        uint32_t sideCount
    );

    void SetFrustum(const Camera::Frustum& frustum);
    bool IsVisible();

    void DispatchCulling(CommandList& commandList);
    void Draw(CommandList& commandList);

private:
    uint32_t m_TotalCount;
    uint32_t m_FrameIndex = 0;
    Camera::Frustum m_Frustum{};

    std::shared_ptr<CommonRootSignature> m_RootSignature;
    Microsoft::WRL::ComPtr<ID3D12CommandSignature> m_CommandSignature;

    std::shared_ptr<StructuredBuffer> m_ModelsStructuredBuffer;
    std::shared_ptr<StructuredBuffer> m_MaterialsStructuredBuffer;
    std::shared_ptr<StructuredBuffer> m_InputCommandsBuffer;
    std::shared_ptr<StructuredBuffer> m_PositionsBuffer;
    std::shared_ptr<StructuredBuffer> m_OutputCommandsBuffers[Window::BUFFER_COUNT];
};
