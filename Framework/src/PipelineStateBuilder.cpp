#include "PipelineStateBuilder.h"
#include <DX12Library/Helpers.h>
#include <Framework/Mesh.h>

static constexpr UINT MAX_RENDER_TARGETS = _countof(D3D12_RT_FORMAT_ARRAY::RTFormats);

PipelineStateBuilder::PipelineStateBuilder(const std::shared_ptr<RootSignature> rootSignature)
    : m_RootSignature(rootSignature)
    , m_InputLayout(VertexAttributes::INPUT_ELEMENT_COUNT)
{
    memcpy(m_InputLayout.data(), VertexAttributes::INPUT_ELEMENTS, sizeof(VertexAttributes::INPUT_ELEMENTS));
}

Microsoft::WRL::ComPtr<ID3D12PipelineState> PipelineStateBuilder::Build(Microsoft::WRL::ComPtr<ID3D12Device2> device) const
{
    Assert(m_VertexShader != nullptr, "Vertex Shader cannot be null.");
    Assert(m_PixelShader != nullptr, "Pixel Shader cannot be null.");

    // Setup the pipeline state.
    struct PipelineStateStream
    {
        CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE RootSignature;
        CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
        CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
        CD3DX12_PIPELINE_STATE_STREAM_VS Vs;
        CD3DX12_PIPELINE_STATE_STREAM_PS Ps;
        CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RtvFormats;
        CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DsvFormat;
        CD3DX12_PIPELINE_STATE_STREAM_BLEND_DESC Blend;
        CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL DepthStencil;
        CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER Rasterizer;
        CD3DX12_PIPELINE_STATE_STREAM_SAMPLE_DESC SampleDesc;
    } pipelineStateStream;

    D3D12_RT_FORMAT_ARRAY rtvFormats = {};
    rtvFormats.NumRenderTargets = static_cast<UINT>(m_RenderTargetFormats.size());
    memcpy(rtvFormats.RTFormats, m_RenderTargetFormats.data(), m_RenderTargetFormats.size() * sizeof(DXGI_FORMAT));

    pipelineStateStream.RootSignature = m_RootSignature->GetRootSignature().Get();

    pipelineStateStream.InputLayout = { m_InputLayout.data(), static_cast<UINT>(m_InputLayout.size()) };
    pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipelineStateStream.Vs = { m_VertexShader->GetBufferPointer(), m_VertexShader->GetBufferSize() };
    pipelineStateStream.Ps = { m_PixelShader->GetBufferPointer(), m_PixelShader->GetBufferSize() };
    pipelineStateStream.DsvFormat = m_DepthStencilFormat;
    pipelineStateStream.RtvFormats = rtvFormats;
    pipelineStateStream.Blend = m_BlendDesc;
    pipelineStateStream.Rasterizer = m_RasterizerDesc;

    // if depth-stencil format is unknown, disable the depth-stencil unit
    pipelineStateStream.DepthStencil = m_DepthStencilFormat != DXGI_FORMAT_UNKNOWN ?
        m_DepthStencilDesc :
        CD3DX12_DEPTH_STENCIL_DESC()
        ;

    pipelineStateStream.SampleDesc = m_SampleDesc;

    const D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
        sizeof(PipelineStateStream), &pipelineStateStream
    };

    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
    ThrowIfFailed(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&pipelineState)));
    return pipelineState;
}

PipelineStateBuilder& PipelineStateBuilder::WithRenderTargetFormats(const std::vector<DXGI_FORMAT>& renderTargetFormats, DXGI_FORMAT depthStencilFormat)
{
    Assert(renderTargetFormats.size() < MAX_RENDER_TARGETS, "Too many render target formats.");

    m_RenderTargetFormats = renderTargetFormats;
    m_DepthStencilFormat = depthStencilFormat;
    return *this;
}

PipelineStateBuilder& PipelineStateBuilder::WithSampleDesc(const DXGI_SAMPLE_DESC& sampleDesc)
{
    m_SampleDesc = sampleDesc;
    return *this;
}

PipelineStateBuilder& PipelineStateBuilder::WithShaders(const Microsoft::WRL::ComPtr<ID3DBlob>& vertexShader, const Microsoft::WRL::ComPtr<ID3DBlob>& pixelShader)
{
    Assert(vertexShader != nullptr, "Vertex Shader cannot be null.");
    Assert(pixelShader != nullptr, "Pixel Shader cannot be null.");

    m_VertexShader = vertexShader;
    m_PixelShader = pixelShader;
    return *this;
}

PipelineStateBuilder& PipelineStateBuilder::WithBlend(const CD3DX12_BLEND_DESC& blendDesc)
{
    m_BlendDesc = blendDesc;
    return *this;
}

PipelineStateBuilder& PipelineStateBuilder::WithAdditiveBlend()
{
    CD3DX12_BLEND_DESC blendDesc = CD3DX12_BLEND_DESC(CD3DX12_DEFAULT());
    auto& rtBlendDesc = blendDesc.RenderTarget[0];
    rtBlendDesc.BlendEnable = true;
    rtBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
    rtBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    rtBlendDesc.SrcBlend = D3D12_BLEND_ONE;
    rtBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
    rtBlendDesc.DestBlend = D3D12_BLEND_ONE;
    rtBlendDesc.DestBlendAlpha = D3D12_BLEND_ONE;
    return WithBlend(blendDesc);
}

PipelineStateBuilder& PipelineStateBuilder::WithDepthStencil(const CD3DX12_DEPTH_STENCIL_DESC& depthStencil)
{
    m_DepthStencilDesc = depthStencil;
    return *this;
}

PipelineStateBuilder& PipelineStateBuilder::WithDisabledDepthStencil()
{
    return WithDepthStencil({});
}

PipelineStateBuilder& PipelineStateBuilder::WithInputLayout(const std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout)
{
    m_InputLayout = inputLayout;
    return *this;
}

PipelineStateBuilder& PipelineStateBuilder::WithRasterizer(const CD3DX12_RASTERIZER_DESC& rasterizer)
{
    m_RasterizerDesc = rasterizer;
    return *this;
}
