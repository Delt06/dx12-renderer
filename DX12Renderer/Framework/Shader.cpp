#include "Shader.h"
#include "Helpers.h"
#include <d3dcompiler.h>
#include <Mesh.h>

using namespace Microsoft::WRL;

Shader::~Shader() = default;

void Shader::Init(Microsoft::WRL::ComPtr<IDevice> device, CommandList& commandList)
{
	{
		ComPtr<ID3DBlob> vertexShaderBlob;
		const std::wstring vertexShaderName = GetVertexShaderName();
		ThrowIfFailed(D3DReadFileToBlob(vertexShaderName.c_str(), &vertexShaderBlob));

		ComPtr<ID3DBlob> pixelShaderBlob;
		const std::wstring pixelShaderName = GetPixelShaderName();
		ThrowIfFailed(D3DReadFileToBlob(pixelShaderName.c_str(), &pixelShaderBlob));

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
		} pipelineStateStream;

		D3D12_RT_FORMAT_ARRAY rtvFormats = {};
		rtvFormats.NumRenderTargets = 1;
		rtvFormats.RTFormats[0] = GetRenderTargetFormat();

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

}

void Shader::SetContext(CommandList& commandList) const
{
	commandList.SetGraphicsRootSignature(m_RootSignature);
	commandList.SetPipelineState(m_PipelineState);
}

void Shader::SetRenderTarget(CommandList& commandList, const RenderTarget& renderTarget, bool autoViewport /*= true*/, bool autoScissorRect /*= true*/)
{
	SetRenderTarget(commandList, renderTarget, -1, autoViewport, autoScissorRect);
}

void Shader::SetRenderTarget(CommandList& commandList, const RenderTarget& renderTarget, UINT arrayIndex, bool autoViewport /*= true*/, bool autoScissorRect /*= true*/)
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

Shader::ScissorRect Shader::GetAutoScissorRect()
{
	return CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX);
}

Shader::Viewport Shader::GetAutoViewport(const RenderTarget& renderTarget)
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

void Shader::CombineRootSignatureFlags(D3D12_ROOT_SIGNATURE_FLAGS& flags, const std::vector<RootParameter>& rootParameters)
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

bool Shader::CheckRootParametersVisiblity(const std::vector<RootParameter>& rootParameters, D3D12_SHADER_VISIBILITY shaderVisibility)
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
