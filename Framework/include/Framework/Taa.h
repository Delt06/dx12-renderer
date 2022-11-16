#pragma once

#include <DX12Library/RootSignature.h>
#include <wrl.h>
#include <d3d12.h>
#include <memory>
#include <DX12Library/Texture.h>
#include <DX12Library/RenderTarget.h>
#include <DirectXMath.h>
#include <Framework/Material.h>

class CommandList;
class Mesh;

class TAA
{
public:
    explicit TAA(const std::shared_ptr<CommonRootSignature>& rootSignature, CommandList& commandList, DXGI_FORMAT backBufferFormat, uint32_t width, uint32_t height);

    [[nodiscard]] DirectX::XMFLOAT2 ComputeJitterOffset() const;
    [[nodiscard]] const DirectX::XMMATRIX& GetPreviousViewProjectionMatrix() const;

    void Resolve(CommandList& commandList, const std::shared_ptr<Texture>& currentBuffer, const std::shared_ptr<Texture>& velocityBuffer, float modulationFactor = 0.9f);
    void Resize(uint32_t width, uint32_t height);
    [[nodiscard]] const std::shared_ptr<Texture>& GetResolvedTexture() const;

    [[nodiscard]] DirectX::XMFLOAT2 GetCurrentJitterOffset() const;
    void OnRenderedFrame(const DirectX::XMMATRIX& viewProjectionMatrix);

private:
    RenderTarget m_ResolveRenderTarget;
    std::shared_ptr<CommonRootSignature> m_RootSignature;

    std::shared_ptr<Mesh> m_BlitMesh;
    std::shared_ptr<Material> m_Material;
    std::shared_ptr<Texture> m_HistoryBuffer;

    constexpr static DirectX::XMFLOAT2 JITTER_OFFSETS[]{
        // Quincunx
        {0, 0},
        {0.5f, 0.5f},
        {0.5f, -0.5f},
        {-0.5f, -0.5f},
        {-0.5f, 0.5f},
        // Halton sequence
        /*{0.500000f, 0.333333f},
        {0.250000f, 0.666667f},
        {0.750000f, 0.111111f},
        {0.125000f, 0.444444f},
        {0.625000f, 0.777778f},
        {0.375000f, 0.222222f},
        {0.875000f, 0.555556f},
        {0.062500f, 0.888889f},
        {0.562500f, 0.037037f},
        {0.312500f, 0.370370f},
        {0.812500f, 0.703704f},
        {0.187500f, 0.148148f},
        {0.687500f, 0.481481f},
        {0.437500f, 0.814815f},
        {0.937500f, 0.259259f},
        {0.031250f, 0.592593f},*/
    };
    constexpr static uint32_t JITTER_OFFSETS_COUNT = _countof(JITTER_OFFSETS);
    DirectX::XMMATRIX m_PreviousViewProjectionMatrix;
    uint32_t m_FrameIndex = 0;

    uint32_t m_Width, m_Height;
};

