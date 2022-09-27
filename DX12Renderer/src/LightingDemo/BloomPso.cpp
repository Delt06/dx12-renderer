#include <LightingDemo/BloomPso.h>

#include <DX12Library/CommandList.h>
#include <DX12Library/RenderTarget.h>
#include <Framework/Mesh.h>
#include <d3dx12.h>
#include <DX12Library/Helpers.h>
#include <DX12Library/ShaderUtils.h>

using namespace Microsoft::WRL;
using namespace DirectX;

namespace
{
	CD3DX12_RESOURCE_DESC CreateRenderTargetDesc(DXGI_FORMAT backBufferFormat, uint32_t width, uint32_t height)
	{
		return CD3DX12_RESOURCE_DESC::Tex2D(backBufferFormat,
			width, height,
			1, 1,
			1, 0,
			D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
	}

	constexpr D3D12_ROOT_SIGNATURE_FLAGS DEFAULT_ROOT_SIGNATURE_FLAGS =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

	void CreateRootSignatureAndPipelineState(RootSignature& rootSignature, ComPtr<ID3D12PipelineState>& pipelineState, ComPtr<ID3D12Device2> device, DXGI_FORMAT backBufferFormat, const std::wstring& pixelShaderPath, const CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC& rootSignatureDescription, CD3DX12_BLEND_DESC* blendDesc = nullptr)
	{
		ComPtr<ID3DBlob> vertexShaderBlob = ShaderUtils::LoadShaderFromFile(L"LightingDemo_Blit_VS.cso");
		ComPtr<ID3DBlob> pixelShaderBlob = ShaderUtils::LoadShaderFromFile(pixelShaderPath);

		// Create a root signature.
		D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData;
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
		if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
		{
			featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
		}

		constexpr D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

		rootSignature.SetRootSignatureDesc(rootSignatureDescription.Desc_1_1, featureData.HighestVersion);

		// Setup the pipeline state.
		struct PipelineStateStream
		{
			CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE RootSignature;
			CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
			CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
			CD3DX12_PIPELINE_STATE_STREAM_VS Vs;
			CD3DX12_PIPELINE_STATE_STREAM_PS Ps;
			CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RtvFormats;
			CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER Rasterizer;
			CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL DepthStencil;
			CD3DX12_PIPELINE_STATE_STREAM_BLEND_DESC Blend;
		} pipelineStateStream;

		D3D12_RT_FORMAT_ARRAY rtvFormats = {};
		rtvFormats.NumRenderTargets = 1;
		rtvFormats.RTFormats[0] = backBufferFormat;

		pipelineStateStream.RootSignature = rootSignature.GetRootSignature().Get();
		pipelineStateStream.InputLayout = { VertexAttributes::INPUT_ELEMENTS, VertexAttributes::INPUT_ELEMENT_COUNT };
		pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pipelineStateStream.Vs = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
		pipelineStateStream.Ps = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
		pipelineStateStream.RtvFormats = rtvFormats;
		pipelineStateStream.Rasterizer = CD3DX12_RASTERIZER_DESC(D3D12_FILL_MODE_SOLID, D3D12_CULL_MODE_BACK, FALSE, 0, 0,
			0, TRUE, FALSE, FALSE, 0,
			D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF);

		auto depthStencilDesc = CD3DX12_DEPTH_STENCIL_DESC(CD3DX12_DEFAULT());
		{
			depthStencilDesc.DepthEnable = false;
		}
		pipelineStateStream.DepthStencil = depthStencilDesc;

		pipelineStateStream.Blend = blendDesc != nullptr ? *blendDesc : CD3DX12_BLEND_DESC(CD3DX12_DEFAULT());

		const D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
			sizeof(PipelineStateStream), &pipelineStateStream
		};

		ThrowIfFailed(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&pipelineState)));
	}

	namespace Prefilter
	{
		enum
		{
			SourceTexture = 0,
			Parameters,
			NumRootParameters
		};

		struct ParametersCb
		{
			XMFLOAT4 Filter;
			XMFLOAT2 TexelSize;
			float Intensity;
			float Padding;
		};
	}

	namespace Downsample
	{
		enum
		{
			SourceTexture = 0,
			Parameters,
			NumRootParameters
		};

		struct ParametersCb
		{
			XMFLOAT2 TexelSize;
			float Intensity;
			float _Padding;
		};
	}

	namespace Upsample
	{
		enum
		{
			SourceTexture = 0,
			Parameters,
			NumRootParameters
		};

		struct ParametersCb
		{
			XMFLOAT2 TexelSize;
			float Intensity;
			float _Padding;
		};
	}
}

BloomPso::BloomPso(Microsoft::WRL::ComPtr<ID3D12Device2> device, CommandList& commandList, uint32_t width, uint32_t height, DXGI_FORMAT backBufferFormat)
	: m_ScissorRect(CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX))
	, m_Width(width)
	, m_Height(height)
{
	m_BlitTriangle = Mesh::CreateBlitTriangle(commandList);

	const size_t pyramidSize = 8;

	// create intermediate textures
	{
		for (size_t i = 0; i < pyramidSize - 1; ++i)
		{
			CreateIntermediateTexture(width, height, m_IntermediateTextures, i, L"Bloom Intermediate Texture", backBufferFormat);
		}
	}

	auto pointClampSampler = CD3DX12_STATIC_SAMPLER_DESC(0, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
	auto linearClampSampler = CD3DX12_STATIC_SAMPLER_DESC(0, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

	// Prefilter PSO
	{
		CD3DX12_DESCRIPTOR_RANGE1 sourceRenderTargetDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0);

		CD3DX12_ROOT_PARAMETER1 rootParameters[Prefilter::NumRootParameters];
		rootParameters[Prefilter::SourceTexture].InitAsDescriptorTable(1, &sourceRenderTargetDescriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[Prefilter::Parameters].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);

		CD3DX12_STATIC_SAMPLER_DESC samplers[] = {
			linearClampSampler
		};

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
		rootSignatureDescription.Init_1_1(Prefilter::NumRootParameters, rootParameters, _countof(samplers), samplers,
			DEFAULT_ROOT_SIGNATURE_FLAGS);

		CreateRootSignatureAndPipelineState(m_PrefilterRootSignature, m_PrefilterPipelineState,
			device, backBufferFormat, L"LightingDemo_Bloom_Prefilter_PS.cso",
			rootSignatureDescription
		);
	}

	// Downsample PSO
	{
		CD3DX12_DESCRIPTOR_RANGE1 sourceRenderTargetDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0);

		CD3DX12_ROOT_PARAMETER1 rootParameters[Downsample::NumRootParameters];
		rootParameters[Downsample::SourceTexture].InitAsDescriptorTable(1, &sourceRenderTargetDescriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[Downsample::Parameters].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);

		CD3DX12_STATIC_SAMPLER_DESC samplers[] = {
			linearClampSampler
		};

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
		rootSignatureDescription.Init_1_1(Downsample::NumRootParameters, rootParameters, _countof(samplers), samplers,
			DEFAULT_ROOT_SIGNATURE_FLAGS);

		CreateRootSignatureAndPipelineState(m_DownsampleRootSignature, m_DownsamplePipelineState,
			device, backBufferFormat, L"LightingDemo_Bloom_Downsample_PS.cso",
			rootSignatureDescription
		);
	}

	// Downsample PSO
	{
		CD3DX12_DESCRIPTOR_RANGE1 sourceRenderTargetDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0);

		CD3DX12_ROOT_PARAMETER1 rootParameters[Upsample::NumRootParameters];
		rootParameters[Upsample::SourceTexture].InitAsDescriptorTable(1, &sourceRenderTargetDescriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[Upsample::Parameters].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);

		CD3DX12_STATIC_SAMPLER_DESC samplers[] = {
			linearClampSampler
		};

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
		rootSignatureDescription.Init_1_1(Upsample::NumRootParameters, rootParameters, _countof(samplers), samplers,
			DEFAULT_ROOT_SIGNATURE_FLAGS);

		auto blendDesc = CD3DX12_BLEND_DESC(CD3DX12_DEFAULT());
		{
			auto& renderTargetBlendDesc = blendDesc.RenderTarget[0];
			renderTargetBlendDesc.BlendEnable = TRUE;
			renderTargetBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
			renderTargetBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
			renderTargetBlendDesc.SrcBlend = D3D12_BLEND_ONE;
			renderTargetBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
			renderTargetBlendDesc.DestBlend = D3D12_BLEND_ONE;
			renderTargetBlendDesc.DestBlendAlpha = D3D12_BLEND_ONE;
		}

		CreateRootSignatureAndPipelineState(m_UpsampleRootSignature, m_UpsamplePipelineState,
			device, backBufferFormat, L"LightingDemo_Bloom_Downsample_PS.cso",
			rootSignatureDescription, &blendDesc
		);
	}
}

void BloomPso::Resize(uint32_t width, uint32_t height)
{
	m_Width = width;
	m_Height = height;

	for (size_t i = 0; i < m_IntermediateTextures.size(); ++i)
	{
		uint32_t textureWidth = 0, textureHeight = 0;
		GetIntermediateTextureSize(width, height, i, textureWidth, textureHeight);
		m_IntermediateTextures[i].Resize(textureWidth, textureHeight);
	}
}

void BloomPso::Draw(CommandList& commandList, const Texture& source, const RenderTarget& destination, const Parameters& parameters)
{
	PIXScope(commandList, "Bloom");
	commandList.SetScissorRect(m_ScissorRect);

	const float threshold = parameters.Threshold;
	const float softThreshold = parameters.SoftThreshold;
	const float intensity = parameters.Intensity;

	{
		PIXScope(commandList, "Bloom Prefilter");

		commandList.SetGraphicsRootSignature(m_PrefilterRootSignature);
		commandList.SetPipelineState(m_PrefilterPipelineState);
		commandList.SetRenderTarget(m_IntermediateTextures[0]);

		uint32_t bufferWidth, bufferHeight;
		GetIntermediateTextureSize(m_Width, m_Height, 0, bufferWidth, bufferHeight);
		float fBufferWidth = static_cast<float>(bufferWidth);
		float fBufferHeight = static_cast<float>(bufferHeight);
		commandList.SetViewport(CD3DX12_VIEWPORT(0.0f, 0.0f, fBufferWidth, fBufferHeight));

		commandList.SetShaderResourceView(Prefilter::SourceTexture, 0, source, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		Prefilter::ParametersCb parameters;
		float knee = threshold * softThreshold;
		parameters.Filter = { threshold, threshold - knee, 2.0f * knee, 0.25f / (knee + 0.00001f) };
		parameters.Intensity = intensity;
		parameters.TexelSize = { 1 / fBufferWidth , 1 / fBufferHeight };
		commandList.SetGraphicsDynamicConstantBuffer(Prefilter::Parameters, parameters);

		m_BlitTriangle->Draw(commandList);
	}

	{
		PIXScope(commandList, "Bloom Downsample");

		commandList.SetGraphicsRootSignature(m_DownsampleRootSignature);
		commandList.SetPipelineState(m_DownsamplePipelineState);

		for (size_t i = 1; i < m_IntermediateTextures.size(); ++i)
		{
			auto& currentRenderTarget = m_IntermediateTextures[i];
			commandList.SetRenderTarget(currentRenderTarget);

			uint32_t bufferWidth, bufferHeight;
			GetIntermediateTextureSize(m_Width, m_Height, i, bufferWidth, bufferHeight);
			float fBufferWidth = static_cast<float>(bufferWidth);
			float fBufferHeight = static_cast<float>(bufferHeight);
			commandList.SetViewport(CD3DX12_VIEWPORT(0.0f, 0.0f, fBufferWidth, fBufferHeight));

			const Texture& previousTexture = m_IntermediateTextures[i - 1].GetTexture(Color0);
			commandList.SetShaderResourceView(Downsample::SourceTexture, 0, previousTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

			Downsample::ParametersCb parameters;
			parameters.Intensity = intensity;
			parameters.TexelSize = { 1 / fBufferWidth , 1 / fBufferHeight };
			commandList.SetGraphicsDynamicConstantBuffer(Downsample::Parameters, parameters);

			m_BlitTriangle->Draw(commandList);
		}
	}

	{
		PIXScope(commandList, "Bloom Upsample");

		commandList.SetGraphicsRootSignature(m_UpsampleRootSignature);
		commandList.SetPipelineState(m_UpsamplePipelineState);

		for (long long i = m_IntermediateTextures.size() - 2; i >= 0; --i)
		{
			auto& currentRenderTarget = m_IntermediateTextures[i];
			commandList.SetRenderTarget(currentRenderTarget);

			uint32_t bufferWidth, bufferHeight;
			GetIntermediateTextureSize(m_Width, m_Height, i, bufferWidth, bufferHeight);
			float fBufferWidth = static_cast<float>(bufferWidth);
			float fBufferHeight = static_cast<float>(bufferHeight);
			commandList.SetViewport(CD3DX12_VIEWPORT(0.0f, 0.0f, fBufferWidth, fBufferHeight));

			const Texture& nextTexture = m_IntermediateTextures[i + 1].GetTexture(Color0);
			commandList.SetShaderResourceView(Downsample::SourceTexture, 0, nextTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

			Upsample::ParametersCb parameters;
			parameters.Intensity = intensity;
			parameters.TexelSize = { 1 / fBufferWidth , 1 / fBufferHeight };
			commandList.SetGraphicsDynamicConstantBuffer(Upsample::Parameters, parameters);

			m_BlitTriangle->Draw(commandList);
		}

		// back blit
		{
			commandList.SetRenderTarget(destination, -1, 0, false);

			float fBufferWidth = static_cast<float>(m_Width);
			float fBufferHeight = static_cast<float>(m_Height);
			commandList.SetViewport(CD3DX12_VIEWPORT(0.0f, 0.0f, fBufferWidth, fBufferHeight));

			const Texture& nextTexture = m_IntermediateTextures[0].GetTexture(Color0);
			commandList.SetShaderResourceView(Downsample::SourceTexture, 0, nextTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

			Upsample::ParametersCb parameters;
			parameters.Intensity = intensity;
			parameters.TexelSize = { 1 / fBufferWidth , 1 / fBufferHeight };
			commandList.SetGraphicsDynamicConstantBuffer(Upsample::Parameters, parameters);

			m_BlitTriangle->Draw(commandList);
		}
	}

}

void BloomPso::GetIntermediateTextureSize(uint32_t width, uint32_t height, size_t index, uint32_t& outWidth, uint32_t& outHeight)
{
	outWidth = width;
	outHeight = height;

	for (size_t i = 0; i <= index; i++)
	{
		outWidth = max(1, outWidth >> 1);
		outHeight = max(1, outHeight >> 1);
	}
}

void BloomPso::CreateIntermediateTexture(uint32_t width, uint32_t height, std::vector<RenderTarget>& destinationList, size_t index, std::wstring name, DXGI_FORMAT format)
{
	uint32_t textureWidth, textureHeight;
	GetIntermediateTextureSize(width, height, index, textureWidth, textureHeight);

	auto desc = CreateRenderTargetDesc(format, textureWidth, textureHeight);
	auto intermediateTexture = Texture(desc, nullptr, TextureUsageType::RenderTarget, name);

	auto renderTarget = RenderTarget();
	renderTarget.AttachTexture(Color0, intermediateTexture);
	destinationList.push_back(renderTarget);
}
