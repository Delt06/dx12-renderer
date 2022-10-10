#include <Framework/CompositeEffect.h>
#include <DX12Library/Helpers.h>
#include <Framework/Mesh.h>
#include <DX12Library/ShaderUtils.h>

using namespace Microsoft::WRL;

CompositeEffect::~CompositeEffect() = default;

void CompositeEffect::Init(Microsoft::WRL::ComPtr<IDevice> device, CommandList& commandList)
{
	{
		// Create a root signature.
		D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData;
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
		if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
		{
			featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
		}

		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		const auto rootParameters = GetRootParameters();
		CombineRootSignatureFlags(rootSignatureFlags, rootParameters);

		const auto staticSamples = GetStaticSamplers();

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
		rootSignatureDescription.Init_1_1(
			static_cast<UINT>(rootParameters.size()), rootParameters.data(),
			static_cast<UINT>(staticSamples.size()), staticSamples.data(),
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
			CD3DX12_PIPELINE_STATE_STREAM_BLEND_DESC Blend;
		} pipelineStateStream;

		D3D12_RT_FORMAT_ARRAY rtvFormats = {};
		rtvFormats.NumRenderTargets = 1;
		rtvFormats.RTFormats[0] = GetRenderTargetFormat();

		pipelineStateStream.RootSignature = m_RootSignature.GetRootSignature().Get();

		pipelineStateStream.InputLayout = { VertexAttributes::INPUT_ELEMENTS, VertexAttributes::INPUT_ELEMENT_COUNT };
		pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pipelineStateStream.Vs = GetVertexShaderBytecode();
		pipelineStateStream.Ps = GetPixelShaderBytecode();
		pipelineStateStream.RtvFormats = rtvFormats;
		pipelineStateStream.Blend = GetBlendMode();

		const D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
			sizeof(PipelineStateStream), &pipelineStateStream
		};

		ThrowIfFailed(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&m_PipelineState)));
	}

	OnPostInit(device, commandList);
}

CompositeEffect::BlendMode CompositeEffect::GetBlendMode() const
{
	return BlendMode(CD3DX12_DEFAULT());
}

void CompositeEffect::SetContext(CommandList& commandList) const
{
	commandList.SetGraphicsRootSignature(m_RootSignature);
	commandList.SetPipelineState(m_PipelineState);
}

void CompositeEffect::SetRenderTarget(CommandList& commandList, const RenderTarget& renderTarget, bool autoViewport /*= true*/, bool autoScissorRect /*= true*/)
{
	SetRenderTarget(commandList, renderTarget, -1, autoViewport, autoScissorRect);
}

void CompositeEffect::SetRenderTarget(CommandList& commandList, const RenderTarget& renderTarget, UINT arrayIndex, bool autoViewport /*= true*/, bool autoScissorRect /*= true*/)
{
	commandList.SetRenderTarget(renderTarget, arrayIndex);

	if (autoViewport)
	{
		commandList.SetViewport(GetAutoViewport(renderTarget));
	}

	if (autoScissorRect)
	{
		commandList.SetScissorRect(GetAutoScissorRect());
	}
}

CompositeEffect::ShaderBlob CompositeEffect::LoadShaderFromFile(const std::wstring& fileName)
{
	return ShaderUtils::LoadShaderFromFile(fileName);
}

CompositeEffect::ScissorRect CompositeEffect::GetAutoScissorRect()
{
	return CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX);
}

CompositeEffect::Viewport CompositeEffect::GetAutoViewport(const RenderTarget& renderTarget)
{
	const auto& color0Texture = renderTarget.GetTexture(Color0);
	const auto& depthTexture = renderTarget.GetTexture(DepthStencil);
	if (!color0Texture.IsValid() && !depthTexture.IsValid())
	{
		throw std::exception("Both Color0 and DepthStencil attachment are invalid. Cannot compute viewport.");
	}

	auto destinationDesc = color0Texture.IsValid() ? color0Texture.GetD3D12ResourceDesc() : depthTexture.GetD3D12ResourceDesc();
	return Viewport(0.0f, 0.0f, static_cast<float>(destinationDesc.Width), static_cast<float>(destinationDesc.Height));
}

CompositeEffect::BlendMode CompositeEffect::AdditiveBlend()
{
	BlendMode desc = BlendMode(CD3DX12_DEFAULT());
	auto& rtBlendDesc = desc.RenderTarget[0];
	rtBlendDesc.BlendEnable = true;
	rtBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
	rtBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	rtBlendDesc.SrcBlend = D3D12_BLEND_ONE;
	rtBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
	rtBlendDesc.DestBlend = D3D12_BLEND_ONE;
	rtBlendDesc.DestBlendAlpha = D3D12_BLEND_ONE;
	return desc;
}

void CompositeEffect::CombineRootSignatureFlags(D3D12_ROOT_SIGNATURE_FLAGS& flags, const std::vector<RootParameter>& rootParameters)
{
	if (!CheckRootParametersVisiblity(rootParameters, D3D12_SHADER_VISIBILITY_VERTEX))
	{
		flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS;
	}

	if (!CheckRootParametersVisiblity(rootParameters, D3D12_SHADER_VISIBILITY_GEOMETRY))
	{
		flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
	}

	if (!CheckRootParametersVisiblity(rootParameters, D3D12_SHADER_VISIBILITY_MESH))
	{
		flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS;
	}

	if (!CheckRootParametersVisiblity(rootParameters, D3D12_SHADER_VISIBILITY_DOMAIN))
	{
		flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;
	}

	if (!CheckRootParametersVisiblity(rootParameters, D3D12_SHADER_VISIBILITY_HULL))
	{
		flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;
	}

	if (!CheckRootParametersVisiblity(rootParameters, D3D12_SHADER_VISIBILITY_PIXEL))
	{
		flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;
	}
}

bool CompositeEffect::CheckRootParametersVisiblity(const std::vector<RootParameter>& rootParameters, D3D12_SHADER_VISIBILITY shaderVisibility)
{
	for (const auto& rootParameter : rootParameters)
	{
		if (rootParameter.ShaderVisibility == D3D12_SHADER_VISIBILITY_ALL)
		{
			return true;
		}

		if (rootParameter.ShaderVisibility == shaderVisibility)
		{
			return true;
		}
	}

	return false;
}
