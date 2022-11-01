#pragma once

#include <memory.h>

#include <d3d12.h>
#include <d3dx12.h>
#include <wrl.h>

#include <DX12Library/CommandList.h>

#include "CommonRootSignature.h"
#include "ShaderBlob.h"

class ComputeShader
{
public:
    explicit ComputeShader(const std::shared_ptr<CommonRootSignature>& rootSignature, const ShaderBlob& shader);

    void Bind(CommandList& commandList) const;

private:
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PipelineState;
};
