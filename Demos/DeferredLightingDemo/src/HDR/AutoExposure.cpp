#include <HDR/AutoExposure.h>
#include <DX12Library/CommandList.h>
#include <d3d12.h>
#include <DX12Library/Helpers.h>
#include <DX12Library/StructuredBuffer.h>
#include <DX12Library/ShaderUtils.h>

namespace
{
    namespace BuildLuminanceHistogram
    {
        struct Parameters
        {
            uint32_t InputWidth;
            uint32_t InputHeight;
            float MinLogLuminance;
            float OneOverLogLuminanceRange;
        };
    }

    namespace AverageLuminanceHistogram
    {
        struct Parameters
        {
            uint32_t PixelCount;
            float MinLogLuminance;
            float LogLuminanceRange;
            float DeltaTime;

            float Tau;
            float _Padding[3];
        };
    }
}

AutoExposure::AutoExposure(const std::shared_ptr<CommonRootSignature>& rootSignature, CommandList& commandList)
    : m_RootSignature(rootSignature)
    , m_BuildLuminanceHistogramShader(rootSignature, ShaderBlob(L"HDR_AutoExposure_BuildLuminanceHistogram_CS.cso"))
    , m_AverageLuminanceHistogramShader(rootSignature, ShaderBlob(L"HDR_AutoExposure_AverageLuminanceHistogram_CS.cso"))
{
    auto luminanceHistogramDesc = CD3DX12_RESOURCE_DESC::Buffer(NUM_HISTOGRAM_BINS * sizeof(uint32_t), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    m_LuminanceHistogram = std::make_shared<StructuredBuffer>(luminanceHistogramDesc, NUM_HISTOGRAM_BINS, sizeof(uint32_t), L"Luminance Histogram");
    m_LuminanceHistogram->CreateViews(NUM_HISTOGRAM_BINS, sizeof(uint32_t));

    auto luminanceOutputDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32_FLOAT, 1, 1, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    m_LuminanceOutput = std::make_shared<Texture>(luminanceOutputDesc, nullptr, TextureUsageType::Other, L"Luminance Output");

    D3D12_SUBRESOURCE_DATA luminanceOutputData;
    float data = 0;
    luminanceOutputData.pData = &data;
    luminanceOutputData.RowPitch = sizeof(float);
    commandList.CopyTextureSubresource(*m_LuminanceOutput, 0, 1, &luminanceOutputData);
}

void AutoExposure::Dispatch(CommandList& commandList, const std::shared_ptr<Texture>& hdrTexture, float deltaTime, float tau /*= 1.1f*/)
{
    constexpr auto minLogLuminance = -10.0f;
    constexpr auto maxLogLuminance = 2.0f;
    constexpr auto logLuminanceRange = maxLogLuminance - minLogLuminance;

    auto hdrTextureDesc = hdrTexture->GetD3D12ResourceDesc();

    PIXScope(commandList, "Compute Average Luminance");
    {
        PIXScope(commandList, "Build Luminance Histogram");

        m_BuildLuminanceHistogramShader.Bind(commandList);

        m_RootSignature->SetComputeShaderResourceView(commandList, 0, ShaderResourceView(hdrTexture));
        m_RootSignature->SetUnorderedAccessView(commandList, 0, UnorderedAccessView(m_LuminanceHistogram));

        BuildLuminanceHistogram::Parameters parameters{};
        parameters.InputWidth = static_cast<uint32_t>(hdrTextureDesc.Width);
        parameters.InputHeight = static_cast<uint32_t>(hdrTextureDesc.Height);
        parameters.MinLogLuminance = minLogLuminance;
        parameters.OneOverLogLuminanceRange = 1.0f / logLuminanceRange;
        m_RootSignature->SetComputeConstantBuffer(commandList, parameters);

        commandList.Dispatch(Math::DivideByMultiple(parameters.InputWidth, 16), Math::DivideByMultiple(parameters.InputHeight, 16));
    }

    commandList.UavBarrier(*m_LuminanceHistogram);

    {
        PIXScope(commandList, "Average Luminance Histogram");

        m_AverageLuminanceHistogramShader.Bind(commandList);

        m_RootSignature->SetUnorderedAccessView(commandList, 0, UnorderedAccessView(m_LuminanceHistogram));
        m_RootSignature->SetUnorderedAccessView(commandList, 1, UnorderedAccessView(m_LuminanceOutput));

        AverageLuminanceHistogram::Parameters parameters{};
        parameters.PixelCount = static_cast<uint32_t>(hdrTextureDesc.Width * hdrTextureDesc.Height);
        parameters.MinLogLuminance = minLogLuminance;
        parameters.LogLuminanceRange = logLuminanceRange;
        parameters.DeltaTime = deltaTime;
        parameters.Tau = tau;
        m_RootSignature->SetComputeConstantBuffer(commandList, parameters);

        commandList.Dispatch(1, 1);
    }

    commandList.UavBarrier(*m_LuminanceOutput);
}

const std::shared_ptr<Texture>& AutoExposure::GetLuminanceOutput() const
{
    return m_LuminanceOutput;
}
