#include <LightingDemo/ShadowPassPsoBase.h>

#include <DX12Library/CommandList.h>
#include <Framework/GameObject.h>
#include <DX12Library/Helpers.h>
#include <Framework/Mesh.h>
#include <Framework/Model.h>
#include <DX12Library/ShaderUtils.h>

using namespace DirectX;
using namespace Microsoft::WRL;

ShadowPassPsoBase::ShadowPassPsoBase(ComPtr<ID3D12Device2> device, UINT resolution)

	: m_ShadowPassParameters{}
	, m_Resolution(resolution)
	, m_Viewport(CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(m_Resolution), static_cast<float>(m_Resolution)))
	, m_ScissorRect(CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX))
{
	ComPtr<ID3DBlob> vertexShaderBlob = ShaderUtils::LoadShaderFromFile(L"LightingDemo_ShadowCaster_VS.cso");
	ComPtr<ID3DBlob> pixelShaderBlob = ShaderUtils::LoadShaderFromFile(L"LightingDemo_ShadowCaster_PS.cso");

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
		D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

	CD3DX12_ROOT_PARAMETER1 rootParameters[RootParameter::NumRootParameters];
	rootParameters[RootParameter::ShadowPassParametersCb].InitAsConstantBufferView(
		0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE,
		D3D12_SHADER_VISIBILITY_VERTEX);

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
	rootSignatureDescription.Init_1_1(RootParameter::NumRootParameters, rootParameters, 0, nullptr,
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
		CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DsvFormat;
		CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RtvFormats;
		CD3DX12_PIPELINE_STATE_STREAM_BLEND_DESC Blend;
		CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL DepthStencil;
		CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER Rasterizer;
	} pipelineStateStream;

	// sRGB formats provide free gamma correction!
	constexpr DXGI_FORMAT backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

	D3D12_RT_FORMAT_ARRAY rtvFormats = {};
	rtvFormats.NumRenderTargets = 1;
	rtvFormats.RTFormats[0] = backBufferFormat;

	pipelineStateStream.RootSignature = m_RootSignature.GetRootSignature().Get();
	pipelineStateStream.InputLayout = { VertexAttributes::INPUT_ELEMENTS, VertexAttributes::INPUT_ELEMENT_COUNT };
	pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pipelineStateStream.Vs = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
	pipelineStateStream.Ps = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
	pipelineStateStream.DsvFormat = SHADOW_MAP_FORMAT;
	pipelineStateStream.RtvFormats = rtvFormats;

	// ColorMask 0
	auto blendDesc = CD3DX12_BLEND_DESC(CD3DX12_DEFAULT());
	{
		auto& renderTargetBlendDesc = blendDesc.RenderTarget[0];
		renderTargetBlendDesc.RenderTargetWriteMask = 0;
	}
	pipelineStateStream.Blend = blendDesc;

	auto depthStencilDesc = CD3DX12_DEPTH_STENCIL_DESC(CD3DX12_DEFAULT());
	pipelineStateStream.DepthStencil = depthStencilDesc;

	auto rasterizerDesc = CD3DX12_RASTERIZER_DESC(CD3DX12_DEFAULT());
	//rasterizerDesc.SlopeScaledDepthBias = 1.0f; // TODO: check the effect
	pipelineStateStream.Rasterizer = rasterizerDesc;

	const D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
		sizeof(PipelineStateStream), &pipelineStateStream
	};

	ThrowIfFailed(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&m_PipelineState)));
}

ShadowPassPsoBase::~ShadowPassPsoBase() = default;

void ShadowPassPsoBase::SetContext(CommandList& commandList) const
{
	commandList.SetViewport(m_Viewport);
	commandList.SetScissorRect(m_ScissorRect);

	commandList.SetPipelineState(m_PipelineState);
	commandList.SetGraphicsRootSignature(m_RootSignature);
}

void ShadowPassPsoBase::SetBias(const float depthBias, const float normalBias)
{
	m_ShadowPassParameters.Bias.x = -depthBias;
	m_ShadowPassParameters.Bias.y = -normalBias;
}

void ShadowPassPsoBase::DrawToShadowMap(CommandList& commandList, const GameObject& gameObject) const
{
	ShadowPassParameters parameters = m_ShadowPassParameters;
	const auto worldMatrix = gameObject.GetWorldMatrix();
	parameters.InverseTransposeModel = XMMatrixTranspose(XMMatrixInverse(nullptr, worldMatrix));
	parameters.Model = worldMatrix;

	commandList.SetGraphicsDynamicConstantBuffer(RootParameter::ShadowPassParametersCb, parameters);

	for (auto& mesh : gameObject.GetModel()->GetMeshes())
	{
		mesh->Draw(commandList);
	}
}

XMMATRIX ShadowPassPsoBase::GetShadowViewProjectionMatrix() const
{
	return m_ShadowPassParameters.ViewProjection;
}
