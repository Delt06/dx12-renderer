#include "ComputeShader.h"
#include <DX12Library/Helpers.h>

ComputeShader::ComputeShader(const std::shared_ptr<CommonRootSignature>& rootSignature, const ShaderBlob& shader)
{
    const auto device = Application::Get().GetDevice();

    {
        struct PipelineStateStream
        {
            CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE RootSignature;
            CD3DX12_PIPELINE_STATE_STREAM_CS CS;
        } pipelineStateStream;

        pipelineStateStream.RootSignature = rootSignature->GetRootSignature().Get();

        const auto& blob = shader.GetBlob();
        pipelineStateStream.CS = CD3DX12_SHADER_BYTECODE(blob->GetBufferPointer(), blob->GetBufferSize());

        const D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc{ sizeof(PipelineStateStream), &pipelineStateStream };
        ThrowIfFailed(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&m_PipelineState)));
    }
}

void ComputeShader::Bind(CommandList& commandList) const
{
    commandList.SetPipelineState(m_PipelineState);
}

