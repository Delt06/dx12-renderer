#include "Ssao.h"
#include "Mesh.h"
#include "CommandList.h"
#include <d3dcompiler.h>
#include "Helpers.h"
#include "Texture.h"
#include <random>

using namespace Microsoft::WRL;
using namespace DirectX;

namespace
{
	float LerpUnclamped(float a, float b, float t)
	{
		return a + t * (b - a);
	}

	namespace SsaoPassRootParameters
	{
		enum RootParameters
		{
			CBuffer,
			GBuffer,
			NoiseTexture,
			NumRootParameters
		};

		struct SSAOCBuffer
		{
			static constexpr size_t SAMPLES_COUNT = 64;

			XMFLOAT2 NoiseScale;
			float Radius;
			uint32_t KernelSize;

			float Power;
			float _Padding[3];

			XMMATRIX InverseProjection;
			XMMATRIX InverseView;
			XMMATRIX View;
			XMMATRIX ViewProjection;

			XMFLOAT3 Samples[SAMPLES_COUNT];
		};
	}

	namespace BlurPassRootParameters
	{
		enum RootParameters
		{
			CBuffer,
			SSAO,
			NumRootParameters
		};

		struct BlurCBuffer
		{
			XMFLOAT2 TexelSize;
			float _Padding[2];
		};
	}
}

Ssao::Ssao(Microsoft::WRL::ComPtr<ID3D12Device2> device, CommandList& commandList, DXGI_FORMAT gBufferFormat, uint32_t width, uint32_t height)
	: m_BlitMesh(Mesh::CreateBlitTriangle(commandList))
	, m_ScissorRect(CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX))
{
	DXGI_FORMAT ssaoFormat = DXGI_FORMAT_R8_UNORM;

	// Create SSAO RT
	{
		auto ssaoTextureDesc = CD3DX12_RESOURCE_DESC::Tex2D(ssaoFormat, width, height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
		auto ssaoTexture = Texture(ssaoTextureDesc, nullptr, TextureUsageType::RenderTarget, L"SSAO");
		m_RenderTarget.AttachTexture(Color0, ssaoTexture);
	}

	// SSAO Pass
	{
		ComPtr<ID3DBlob> vertexShaderBlob;
		ThrowIfFailed(D3DReadFileToBlob(L"Blit_VertexShader.cso", &vertexShaderBlob));

		ComPtr<ID3DBlob> pixelShaderBlob;
		ThrowIfFailed(D3DReadFileToBlob(L"SSAO_PixelShader.cso", &pixelShaderBlob));

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

		CD3DX12_DESCRIPTOR_RANGE1 gBufferDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0);
		CD3DX12_DESCRIPTOR_RANGE1 noiseTextureDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);

		CD3DX12_ROOT_PARAMETER1 rootParameters[SsaoPassRootParameters::NumRootParameters];
		rootParameters[SsaoPassRootParameters::CBuffer].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[SsaoPassRootParameters::GBuffer].InitAsDescriptorTable(1, &gBufferDescriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[SsaoPassRootParameters::NoiseTexture].InitAsDescriptorTable(1, &noiseTextureDescriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);

		CD3DX12_STATIC_SAMPLER_DESC samplers[] = {
			// point clamp
			CD3DX12_STATIC_SAMPLER_DESC(0, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP),
			// point wrap
			CD3DX12_STATIC_SAMPLER_DESC(1, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP),
		};

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
		rootSignatureDescription.Init_1_1(SsaoPassRootParameters::NumRootParameters, rootParameters, _countof(samplers), samplers,
			rootSignatureFlags);

		m_SsaoPassRootSignature.SetRootSignatureDesc(rootSignatureDescription.Desc_1_1, featureData.HighestVersion);

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
		rtvFormats.RTFormats[0] = ssaoFormat;

		pipelineStateStream.RootSignature = m_SsaoPassRootSignature.GetRootSignature().Get();

		pipelineStateStream.InputLayout = { VertexAttributes::INPUT_ELEMENTS, VertexAttributes::INPUT_ELEMENT_COUNT };
		pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pipelineStateStream.Vs = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
		pipelineStateStream.Ps = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
		pipelineStateStream.RtvFormats = rtvFormats;

		const D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
			sizeof(PipelineStateStream), &pipelineStateStream
		};

		ThrowIfFailed(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&m_SsaoPassPipelineState)));
	}

	{
		ComPtr<ID3DBlob> vertexShaderBlob;
		ThrowIfFailed(D3DReadFileToBlob(L"Blit_VertexShader.cso", &vertexShaderBlob));

		ComPtr<ID3DBlob> pixelShaderBlob;
		ThrowIfFailed(D3DReadFileToBlob(L"SSAO_Blur_PixelShader.cso", &pixelShaderBlob));

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

		CD3DX12_DESCRIPTOR_RANGE1 ssaoDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

		CD3DX12_ROOT_PARAMETER1 rootParameters[BlurPassRootParameters::NumRootParameters];
		rootParameters[BlurPassRootParameters::CBuffer].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[BlurPassRootParameters::SSAO].InitAsDescriptorTable(1, &ssaoDescriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);

		CD3DX12_STATIC_SAMPLER_DESC samplers[] = {
			// point clamp
			CD3DX12_STATIC_SAMPLER_DESC(0, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP),
		};

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
		rootSignatureDescription.Init_1_1(BlurPassRootParameters::NumRootParameters, rootParameters, _countof(samplers), samplers,
			rootSignatureFlags);

		m_BlurPassRootSignature.SetRootSignatureDesc(rootSignatureDescription.Desc_1_1, featureData.HighestVersion);

		// Setup the pipeline state.
		struct PipelineStateStream
		{
			CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE RootSignature;
			CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
			CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
			CD3DX12_PIPELINE_STATE_STREAM_VS Vs;
			CD3DX12_PIPELINE_STATE_STREAM_PS Ps;
			CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RtvFormats;
			CD3DX12_PIPELINE_STATE_STREAM_BLEND_DESC Blend;
		} pipelineStateStream;

		D3D12_RT_FORMAT_ARRAY rtvFormats = {};
		rtvFormats.NumRenderTargets = 1;
		rtvFormats.RTFormats[0] = gBufferFormat;

		pipelineStateStream.RootSignature = m_BlurPassRootSignature.GetRootSignature().Get();

		pipelineStateStream.InputLayout = { VertexAttributes::INPUT_ELEMENTS, VertexAttributes::INPUT_ELEMENT_COUNT };
		pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pipelineStateStream.Vs = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
		pipelineStateStream.Ps = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
		pipelineStateStream.RtvFormats = rtvFormats;

		// blend AO from GBuffer and SSAO
		CD3DX12_BLEND_DESC blendDesc{};
		auto& rtBlendDesc = blendDesc.RenderTarget[0];
		rtBlendDesc.BlendEnable = true;
		rtBlendDesc.BlendOp = D3D12_BLEND_OP_MIN;
		rtBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_MIN;
		rtBlendDesc.DestBlend = D3D12_BLEND_ONE;
		rtBlendDesc.DestBlendAlpha = D3D12_BLEND_ONE;
		rtBlendDesc.SrcBlend = D3D12_BLEND_ONE;
		rtBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
		rtBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_BLUE; // only write to the AO channel
		pipelineStateStream.Blend = blendDesc;

		const D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
			sizeof(PipelineStateStream), &pipelineStateStream
		};

		ThrowIfFailed(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&m_BlurPassPipelineState)));
	}

	std::uniform_real_distribution<float> randomFloats(0.0f, 1.0f);
	std::default_random_engine generator;

	// Generate samples
	{
		constexpr size_t samplesCount = SsaoPassRootParameters::SSAOCBuffer::SAMPLES_COUNT;
		m_Samples.reserve(samplesCount);

		for (size_t i = 0; i < samplesCount; ++i)
		{
			auto sample = XMVectorSet(
				randomFloats(generator) * 2.0f - 1.0f,
				randomFloats(generator) * 2.0f - 1.0f,
				randomFloats(generator),
				0.0f
			);
			sample = XMVector3Normalize(sample);
			sample *= randomFloats(generator);

			float scale = static_cast<float>(i) / static_cast<float>(samplesCount);
			scale = LerpUnclamped(0.1f, 1.0f, scale * scale);
			sample *= scale;

			XMFLOAT3 sampleF3{};
			XMStoreFloat3(&sampleF3, sample);
			m_Samples.push_back(sampleF3);
		}
	}

	// Generate Noise Texture
	{
		constexpr uint32_t noiseTextureWidth = 4;
		constexpr uint32_t noiseTextureHeight = 4;
		constexpr auto totalNoiseSamples = noiseTextureWidth * noiseTextureHeight;

		std::vector<XMFLOAT2> noiseSamples;
		noiseSamples.reserve(totalNoiseSamples);

		for (uint32_t i = 0; i < totalNoiseSamples; ++i)
		{
			XMFLOAT2 noiseSample = {
				randomFloats(generator) * 2.0f - 1.0f,
				randomFloats(generator) * 2.0f - 1.0f
			};
			noiseSamples.push_back(noiseSample);
		}

		auto noiseTextureDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G32_FLOAT, noiseTextureWidth, noiseTextureHeight, 1, 1);
		m_NoiseTexture = Texture(noiseTextureDesc, nullptr, TextureUsageType::Other, L"SSAO Noise Texture");
		D3D12_SUBRESOURCE_DATA subresourceData{};
		subresourceData.pData = noiseSamples.data();
		subresourceData.RowPitch = sizeof(XMFLOAT2) * noiseTextureWidth;
		commandList.CopyTextureSubresource(m_NoiseTexture, 0, 1, &subresourceData);
	}
}

void Ssao::Resize(uint32_t width, uint32_t height)
{
	m_RenderTarget.Resize(width, height);
}

void Ssao::SsaoPass(CommandList& commandList, const Texture& gBufferNormals, const Texture& gBufferDepth, DirectX::XMMATRIX viewMatrix, DirectX::XMMATRIX projectionMatrix, const D3D12_SHADER_RESOURCE_VIEW_DESC* gBufferDepthSrvDesc, float radius, float power)
{
	PIXScope(commandList, "SSAO Pass");

	commandList.SetGraphicsRootSignature(m_SsaoPassRootSignature);
	commandList.SetPipelineState(m_SsaoPassPipelineState);
	commandList.SetScissorRect(m_ScissorRect);
	

	const auto& ssaoTexture = m_RenderTarget.GetTexture(Color0);
	const auto ssaoTextureDesc = ssaoTexture.GetD3D12ResourceDesc();
	D3D12_VIEWPORT viewport(0.0f, 0.0f, static_cast<float>(ssaoTextureDesc.Width), static_cast<float>(ssaoTextureDesc.Height));
	commandList.SetViewport(viewport);
	commandList.SetRenderTarget(m_RenderTarget);

	const auto noiseTextureDesc = m_NoiseTexture.GetD3D12ResourceDesc();
	SsaoPassRootParameters::SSAOCBuffer cbuffer{};
	cbuffer.NoiseScale = {
		static_cast<float>(ssaoTextureDesc.Width) / static_cast<float>(noiseTextureDesc.Width),
		static_cast<float>(ssaoTextureDesc.Height) / static_cast<float>(noiseTextureDesc.Height),
	};
	cbuffer.Radius = radius;
	cbuffer.KernelSize = SsaoPassRootParameters::SSAOCBuffer::SAMPLES_COUNT;
	cbuffer.Power = power;
	cbuffer.InverseProjection = XMMatrixInverse(nullptr, projectionMatrix);
	cbuffer.InverseView = XMMatrixInverse(nullptr, viewMatrix);
	cbuffer.View = viewMatrix;
	cbuffer.ViewProjection = viewMatrix * projectionMatrix;
	memcpy(cbuffer.Samples, m_Samples.data(), sizeof(cbuffer.Samples));
	commandList.SetGraphicsDynamicConstantBuffer(SsaoPassRootParameters::CBuffer, cbuffer);

	commandList.SetShaderResourceView(SsaoPassRootParameters::GBuffer, 0, gBufferNormals, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	commandList.SetShaderResourceView(SsaoPassRootParameters::GBuffer, 1, gBufferDepth, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0, 1, gBufferDepthSrvDesc);

	commandList.SetShaderResourceView(SsaoPassRootParameters::NoiseTexture, 0, m_NoiseTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	m_BlitMesh->Draw(commandList);
}

void Ssao::BlurPass(CommandList& commandList, const RenderTarget& surfaceRenderTarget)
{
	PIXScope(commandList, "Blur Pass");

	commandList.SetGraphicsRootSignature(m_BlurPassRootSignature);
	commandList.SetPipelineState(m_BlurPassPipelineState);
	commandList.SetScissorRect(m_ScissorRect);
	
;	const auto rtDesc = surfaceRenderTarget.GetTexture(Color0).GetD3D12ResourceDesc();
	D3D12_VIEWPORT viewport(0.0f, 0.0f, static_cast<float>(rtDesc.Width), static_cast<float>(rtDesc.Height));
	commandList.SetViewport(viewport);
	commandList.SetRenderTarget(surfaceRenderTarget);

	const auto& ssaoTexture = m_RenderTarget.GetTexture(Color0);
	const auto ssaoTextureDesc = ssaoTexture.GetD3D12ResourceDesc();
	BlurPassRootParameters::BlurCBuffer cbuffer{};
	cbuffer.TexelSize = {
		1.0f / static_cast<float>(ssaoTextureDesc.Width),
		1.0f / static_cast<float>(ssaoTextureDesc.Height),
	};
	commandList.SetGraphicsDynamicConstantBuffer(BlurPassRootParameters::CBuffer, cbuffer);

	commandList.SetShaderResourceView(BlurPassRootParameters::SSAO, 0, ssaoTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	m_BlitMesh->Draw(commandList);
}
