#pragma once

#include <cstdint>

#include <d3d12.h>
#include <wrl.h>

#include <DX12Library/CommandList.h>
#include <DX12Library/StructuredBuffer.h>

#include <Framework/CommonRootSignature.h>
#include <Framework/Material.h>

struct MeshletDrawIndirectCommand
{
    uint32_t m_MeshletIndex;
    uint32_t m_Flags;
    D3D12_DRAW_ARGUMENTS m_DrawArguments;
};

class MeshletDrawIndirect
{
public:
    MeshletDrawIndirect(const std::shared_ptr<CommonRootSignature>& pRootSignature, const std::shared_ptr<Material>& pDrawMaterial);

    void DrawIndirect(CommandList& commandList, uint32_t maxMeshlets, StructuredBuffer& commandsBuffer) const;

private:
    Microsoft::WRL::ComPtr<ID3D12CommandSignature> m_CommandSignature;
    std::shared_ptr<Material> m_DrawMaterial;
};
