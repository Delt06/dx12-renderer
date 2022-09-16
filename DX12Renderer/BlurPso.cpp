#include "BlurPso.h"
#include "Texture.h"
#include "RenderTarget.h"
#include <DirectXMath.h>
#include <Mesh.h>
#include <d3dcompiler.h>
#include <Helpers.h>

using namespace Microsoft::WRL;
using namespace DirectX;

namespace
{
	namespace RootParameters
	{
		enum RootParameters
		{
			// Texture2D : register(t0)
			Source,
			// ConstantBuffer : register(b0)
			BlurParametersCb,
			NumRootParameters
		};
	}

	struct BlurParameters
	{
		XMFLOAT2 OneOverTexelSize;
		XMFLOAT2 Direction;
	};
}

BlurPso::BlurPso(Microsoft::WRL::ComPtr<ID3D12Device2> device, CommandList& commandList, DXGI_FORMAT renderTargetFormat, UINT resolutionScaling, float spread)
	: m_ScissorRect(CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX))
	, m_RenderTargetFormat(renderTargetFormat)
	, m_ResolutionScaling(resolutionScaling)
	, m_Spread(spread)
{
	// create root signature and PSO
	{
		ComPtr<ID3DBlob> vertexShaderBlob;
		ThrowIfFailed(D3DReadFileToBlob(L"Blit_VertexShader.cso", &vertexShaderBlob));

		ComPtr<ID3DBlob> pixelShaderBlob;
		ThrowIfFailed(D3DReadFileToBlob(L"Blur_PixelShader.cso", &pixelShaderBlob));

		// Create a root signature.
		D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData;
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
		if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
		{
			featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
		}

		constexpr D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS;

		CD3DX12_DESCRIPTOR_RANGE1 sourceDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

		CD3DX12_ROOT_PARAMETER1 rootParameters[RootParameters::NumRootParameters];
		rootParameters[RootParameters::Source].InitAsDescriptorTable(1, &sourceDescriptorRange, D3D12_SHADER_VISIBILITY_PIXEL); 
		rootParameters[RootParameters::BlurParametersCb].InitAsConstants(sizeof(BlurParameters) / sizeof(float), 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);

		CD3DX12_STATIC_SAMPLER_DESC samplers[] = {
			// linear clamp
			CD3DX12_STATIC_SAMPLER_DESC(0, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP),
		};

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
		rootSignatureDescription.Init_1_1(RootParameters::NumRootParameters, rootParameters, _countof(samplers), samplers,
			rootSignatureFlags);

		m_RootSignature.SetRootSignatureDesc(rootSignatureDescription.Desc_1_1, featureData.HighestVersion);

		// Setup the pipeline state.
		struct PipelineStateStream
		{
			CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE RootSignature;
			CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
			CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
			CD3DX12_PIPELINE_STATE_STREAM_VS Vs;
			CD3DX12_PIPELINE_STATE_STREAM_PS Ps;
			CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RtvFormats;
		} pipelineStateStream;

		D3D12_RT_FORMAT_ARRAY rtvFormats = {};
		rtvFormats.NumRenderTargets = 1;
		rtvFormats.RTFormats[0] = renderTargetFormat;

		pipelineStateStream.RootSignature = m_RootSignature.GetRootSignature().Get();

		pipelineStateStream.InputLayout = { VertexAttributes::INPUT_ELEMENTS, VertexAttributes::INPUT_ELEMENT_COUNT };
		pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pipelineStateStream.Vs = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
		pipelineStateStream.Ps = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
		pipelineStateStream.RtvFormats = rtvFormats;

		const D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
			sizeof(PipelineStateStream), &pipelineStateStream
		};

		ThrowIfFailed(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&m_PipelineState)));
	}

	m_BlitMesh = Mesh::CreateBlitTriangle(commandList);
}

Texture BlurPso::Blur(CommandList& commandList, Texture& source, BlurDirection direction, const D3D12_SHADER_RESOURCE_VIEW_DESC* sourceDesc /*= nullptr*/)
{
	// prepare
	const auto sourceWidth = source.GetD3D12ResourceDesc().Width;
	const auto sourceHeight = source.GetD3D12ResourceDesc().Height;

	const auto resultWidth = max(1, (sourceWidth / m_ResolutionScaling));
	const auto resultHeight = max(1, (sourceHeight / m_ResolutionScaling));

	auto colorDesc = CD3DX12_RESOURCE_DESC::Tex2D(m_RenderTargetFormat,
		resultWidth, resultHeight,
		1, 1,
		1, 0,
		D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

	D3D12_CLEAR_VALUE colorClearValue;
	colorClearValue.Format = m_RenderTargetFormat;

	auto resultTexture = Texture(colorDesc, &colorClearValue,
		TextureUsageType::RenderTarget,
		L"Blur-Temp");

	RenderTarget renderTarget;
	renderTarget.AttachTexture(Color0, resultTexture);

	// render
	{
		PIXScope(commandList, "Blur");

		commandList.SetGraphicsRootSignature(m_RootSignature);
		commandList.SetPipelineState(m_PipelineState);
		commandList.SetRenderTarget(renderTarget);

		CD3DX12_VIEWPORT viewport(0.0f, 0.0f, static_cast<float>(resultWidth), static_cast<float>(resultHeight));
		commandList.SetViewport(viewport);
		commandList.SetScissorRect(m_ScissorRect);

		commandList.SetShaderResourceView(RootParameters::Source, 0, source, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0, UINT_MAX, sourceDesc);
		
		BlurParameters blurParameters{};
		blurParameters.OneOverTexelSize.x = m_Spread / static_cast<float>(sourceWidth);
		blurParameters.OneOverTexelSize.y = m_Spread / static_cast<float>(sourceHeight);

		switch (direction)
		{
		case BlurDirection::Horizontal:
			blurParameters.Direction = { 1, 0 };
			break;
		case BlurDirection::Vertical:
			blurParameters.Direction = { 0, 1 };
			break;
		}
		
		commandList.SetGraphics32BitConstants(RootParameters::BlurParametersCb, blurParameters);

		m_BlitMesh->Draw(commandList);
	}

	return resultTexture;
}


