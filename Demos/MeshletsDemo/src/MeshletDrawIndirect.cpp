#include "MeshletDrawIndirect.h"

#include "DX12Library/Helpers.h"

#include "Framework/Mesh.h"

namespace
{
    auto CreateCommandSignature(const std::shared_ptr<CommonRootSignature>& pRootSignature)
    {
        D3D12_INDIRECT_ARGUMENT_DESC argumentDescs[2] = {};
        argumentDescs[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT;
        argumentDescs[0].Constant.DestOffsetIn32BitValues = 0;
        argumentDescs[0].Constant.Num32BitValuesToSet = 2;
        argumentDescs[0].Constant.RootParameterIndex = CommonRootSignature::RootParameters::Constants;
        argumentDescs[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;

        D3D12_COMMAND_SIGNATURE_DESC commandSignatureDesc = {};
        commandSignatureDesc.pArgumentDescs = argumentDescs;
        commandSignatureDesc.NumArgumentDescs = _countof(argumentDescs);
        commandSignatureDesc.ByteStride = sizeof(MeshletDrawIndirectCommand);

        const auto pDevice = Application::Get().GetDevice();
        Microsoft::WRL::ComPtr<ID3D12CommandSignature> commandSignature;
        ThrowIfFailed(pDevice->CreateCommandSignature(&commandSignatureDesc, pRootSignature->GetRootSignature().Get(), IID_PPV_ARGS(&commandSignature)));
        ThrowIfFailed(commandSignature->SetName(L"Meshlet Draw Indirect Command Signature"));
        return commandSignature;
    }
}

MeshletDrawIndirect::MeshletDrawIndirect(const std::shared_ptr<CommonRootSignature>& pRootSignature, const std::shared_ptr<Material>& pDrawMaterial)
    : m_CommandSignature(CreateCommandSignature(pRootSignature))
    , m_DrawMaterial(pDrawMaterial)
{}

void MeshletDrawIndirect::DrawIndirect(CommandList& commandList, const uint32_t maxMeshlets, StructuredBuffer& commandsBuffer) const
{
    m_DrawMaterial->Bind(commandList);

    commandList.SetPrimitiveTopology(Mesh::PRIMITIVE_TOPOLOGY);

    commandList.DrawIndirect(
        m_CommandSignature,
        maxMeshlets,
        commandsBuffer.GetD3D12Resource(),
        0,
        commandsBuffer.GetCounterBuffer().GetD3D12Resource()
    );

    m_DrawMaterial->Unbind(commandList);
}
