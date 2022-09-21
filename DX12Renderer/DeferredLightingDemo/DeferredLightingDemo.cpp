#include "DeferredLightingDemo.h"

#include <Application.h>
#include <CommandQueue.h>
#include <CommandList.h>
#include <Helpers.h>
#include <Light.h>
#include <Material.h>
#include <Window.h>
#include <GameObject.h>
#include <Bone.h>
#include <Animation.h>
#include <Cubemap.h>

#include <wrl.h>

#include "GraphicsSettings.h"
#include "Model.h"
#include "ModelLoader.h"

using namespace Microsoft::WRL;

#include <d3d12.h>
#include <d3dx12.h>
#include <d3dcompiler.h>
#include <DirectXColors.h>
#include <MatricesCb.h>

using namespace DirectX;

#include <algorithm> // For std::min and std::max.
#if defined(min)
#undef min
#endif

#include "PBR/IBL/DiffuseIrradiance.h"
#include "PBR/PbrTextureLoader.h"
#include "PBR/IBL/BrdfIntegration.h"
#include "PBR/IBL/PreFilterEnvironment.h"
#include "Blit.h"
#include "HDR/AutoExposure.h"
#include "TaaCBuffer.h"
#include "Taa.h"

#if defined(max)
#undef max
#endif

namespace
{
	// Clamp a value between a min and max range.
	template <typename T>
	constexpr const T& Clamp(const T& val, const T& min, const T& max)
	{
		return val < min ? min : val > max ? max : val;
	}

	template<typename T>
	constexpr const T& Remap(const T& low1, const T& high1, const T& low2, const T& high2, const T& value)
	{
		return low2 + (value - low1) * (high2 - low2) / (high1 - low1);
	}

	template<typename T>
	constexpr T Smoothstep(const T& edge0, const T& edge1, const T& x)
	{
		auto t = Clamp<T>((x - edge0) / (edge1 - edge0), 0, 1);
		return t * t * (3 - 2 * t);
	}

	XMFLOAT4 NormalizeColor(XMFLOAT4 value)
	{
		auto vector = XMLoadFloat4(&value);
		vector = XMVector3Normalize(vector);
		XMFLOAT4 result{};
		XMStoreFloat4(&result, vector);
		return result;
	}

	float GetColorMagnitude(XMFLOAT4 value)
	{
		auto vector = XMLoadFloat4(&value);
		auto magnitude = XMVector3Length(vector);
		float result = 0;
		XMStoreFloat(&result, magnitude);
		return result;
	}

	bool allowFullscreenToggle = true;
	constexpr FLOAT CLEAR_COLOR[] = { 0.4f, 0.6f, 0.9f, 1.0f };
	constexpr FLOAT VELOCITY_CLEAR_COLOR[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	constexpr FLOAT LIGHT_BUFFER_CLEAR_COLOR[] = { 0.0f, 0.0f, 0.0f, 0.0f };

	// Builds a look-at (world) matrix from a point, up and direction vectors.
	XMMATRIX XM_CALLCONV LookAtMatrix(FXMVECTOR position, FXMVECTOR direction, FXMVECTOR up)
	{
		assert(!XMVector3Equal(direction, XMVectorZero()));
		assert(!XMVector3IsInfinite(direction));
		assert(!XMVector3Equal(up, XMVectorZero()));
		assert(!XMVector3IsInfinite(up));

		XMVECTOR R2 = XMVector3Normalize(direction);

		XMVECTOR R0 = XMVector3Cross(up, R2);
		R0 = XMVector3Normalize(R0);

		XMVECTOR R1 = XMVector3Cross(R2, R0);

		XMMATRIX M(R0, R1, R2, position);

		return M;
	}

	struct BoneSbItem
	{
		XMMATRIX Transform;
	};

	namespace GBufferRootParameters
	{
		enum RootParameters
		{
			// ConstantBuffer : register(b0);
			MatricesCb,
			// ConstantBuffer : register(b1);
			MaterialCb,
			// ConstantBuffer : register(b1, space1);
			TaaBufferCb,
			// Texture2D register(t0-t1);
			Textures,
			NumRootParameters
		};
	}

	namespace DirectionalLightBufferRootParameters
	{
		enum RootParameters
		{
			// ConstantBuffer: register(b0);
			MatricesCb,
			// ConstantBuffer : register(b1);
			DirectionalLightCb,
			// ConstantBuffer : register(b2);
			ScreenParametersCb,
			// Texture2D : register(t0-t2);
			GBuffer,
			// TextureCube : register(t3)
			Ambient,
			NumRootParameters
		};
	}

	namespace LightStencilRootParameters
	{
		enum RootParameters
		{
			// ConstantBuffer: register(b0);
			MatricesCb,
			NumRootParameters
		};
	}

	namespace LightPassRootParameters
	{
		enum RootParameters
		{
			// ConstantBuffer: register(b0);
			MatricesCb,
			// ConstantBuffer : register(b1);
			LightCb,
			// ConstantBuffer : register(b2);
			ScreenParametersCb,
			// Texture2D register(t0-t1);
			GBuffer,
			NumRootParameters
		};
	}

	namespace SkyboxPassRootParameters
	{
		enum RootParameters
		{
			// ConstantBuffer: register(b0);
			MatricesCb,
			// TextureCube : register(t0
			SkyboxCubemap,
			NumRootParameters
		};
	}

	CD3DX12_BLEND_DESC AdditiveBlending()
	{
		CD3DX12_BLEND_DESC desc = CD3DX12_BLEND_DESC(CD3DX12_DEFAULT());
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

	float GetMaxColorComponentRGB(XMFLOAT4 color)
	{
		return std::max(color.x, std::max(color.y, color.z));
	}

	XMMATRIX GetModelMatrix(const PointLight& light)
	{
		// assuming minimum visible attenuation of L...
		// 1/(c+l*d+q*d*d) = L
		const auto l = 0.02f / GetMaxColorComponentRGB(light.Color);
		const auto lInv = 1 / l;
		auto discriminant = light.LinearAttenuation * light.LinearAttenuation - 4 * light.QuadraticAttenuation * (light.ConstantAttenuation - lInv);
		auto radius = (-light.LinearAttenuation + sqrt(discriminant)) / (2 * light.QuadraticAttenuation);
		return XMMatrixScaling(radius, radius, radius) *
			XMMatrixTranslationFromVector(XMLoadFloat4(&light.PositionWs));
	}

	XMMATRIX GetModelMatrix(const SpotLight& light)
	{
		// assuming minimum visible attenuation of L...
		// 1/(1+a * d * d) = L
		const auto l = 0.01f / (light.Intensity * GetMaxColorComponentRGB(light.Color));
		const auto lInv = 1 / l;
		auto scaleZ = (light.Attenuation > 0.0f) ? (sqrt((lInv - 1) / light.Attenuation)) : 1000.0f;
		auto tangent = static_cast<float>(tan(light.SpotAngle));
		auto scaleX = 2 * scaleZ * tangent;

		// generate any suitable up vector
		auto direction = XMLoadFloat4(&light.DirectionWs);
		auto bitangent = XMLoadFloat4(&light.DirectionWs);
		bitangent += XMVectorSet(0.1f, 0.1f, 0.1f, 0); // slightly change to produce a non-parallel vector
		bitangent = XMVector3Normalize(bitangent);
		auto up = XMVector3Cross(direction, bitangent);

		return XMMatrixScaling(scaleX, scaleX, scaleZ) *
			XMMatrixInverse(nullptr, XMMatrixLookToLH(XMLoadFloat4(&light.PositionWs), direction, up));
	}

	XMMATRIX GetModelMatrix(const CapsuleLight& light)
	{
		// assuming minimum visible attenuation of L...
		// 1/(1+a * d * d) = L
		const auto l = 0.05f / (GetMaxColorComponentRGB(light.Color));
		const auto lInv = 1 / l;
		auto radius = (light.Attenuation > 0.0f) ? (sqrt((lInv - 1) / light.Attenuation)) : 1000.0f;

		auto pointA = XMLoadFloat4(&light.PointA);
		auto pointB = XMLoadFloat4(&light.PointB);

		auto origin = (pointA + pointB) * 0.5f;
		auto ab = pointB - pointA;
		auto lengthV = XMVector3Length(ab);
		float length;
		XMStoreFloat(&length, lengthV);
		auto up = ab / length;
		auto upModified = XMVector3Normalize(up + XMVectorSet(0.1f, 0.1f, 0.1f, 0));
		auto forward = XMVector3Normalize(XMVector3Cross(up, upModified));

		XMMATRIX transform = XMMatrixScaling(radius, length + radius * 2, radius) *
			XMMatrixInverse(nullptr, XMMatrixLookToLH(origin, forward, up));
		return transform;
	}

	auto GetSkyboxSampler(UINT shaderRegister)
	{
		auto skyboxSampler = CD3DX12_STATIC_SAMPLER_DESC(shaderRegister, D3D12_FILTER_MAXIMUM_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
		skyboxSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		return skyboxSampler;
	}
}

DeferredLightingDemo::DeferredLightingDemo(const std::wstring& name, int width, int height, GraphicsSettings graphicsSettings)
	: Base(name, width, height, graphicsSettings.VSync)
	, m_Viewport(CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)))
	, m_ScissorRect(CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX))
	, m_GraphicsSettings(graphicsSettings)
	, m_CameraController{}
	, m_Width(0)
	, m_Height(0)
{
	const XMVECTOR cameraPos = XMVectorSet(0, 5, -20, 1);
	const XMVECTOR cameraTarget = XMVectorSet(0, 5, 0, 1);
	const XMVECTOR cameraUp = XMVectorSet(0, 1, 0, 0);

	m_Camera.SetLookAt(cameraPos, cameraTarget, cameraUp);

	m_PAlignedCameraData = static_cast<CameraData*>(_aligned_malloc(sizeof(CameraData), 16));

	m_PAlignedCameraData->m_InitialPosition = m_Camera.GetTranslation();
	m_PAlignedCameraData->m_InitialQRotation = m_Camera.GetRotation();
}

DeferredLightingDemo::~DeferredLightingDemo()
{
	_aligned_free(m_PAlignedCameraData);
}

bool DeferredLightingDemo::LoadContent()
{
	const auto device = Application::Get().GetDevice();
	const auto commandQueue = Application::Get().GetCommandQueue();
	const auto commandList = commandQueue->GetCommandList();

	// Generate default texture
	{
		m_WhiteTexture2d = std::make_shared<Texture>();
		commandList->LoadTextureFromFile(*m_WhiteTexture2d, L"Assets/Textures/white.png");
	}

	constexpr DXGI_FORMAT gBufferFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
	constexpr DXGI_FORMAT lightBufferFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
	constexpr DXGI_FORMAT resultFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	constexpr DXGI_FORMAT depthBufferFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	constexpr UINT gBufferTexturesCount = 5;

	D3D12_CLEAR_VALUE gBufferColorClearValue;
	gBufferColorClearValue.Format = gBufferFormat;
	memcpy(gBufferColorClearValue.Color, CLEAR_COLOR, sizeof CLEAR_COLOR);

	D3D12_CLEAR_VALUE velocityColorClearValue;
	velocityColorClearValue.Format = gBufferFormat;
	memcpy(velocityColorClearValue.Color, VELOCITY_CLEAR_COLOR, sizeof VELOCITY_CLEAR_COLOR);

	D3D12_CLEAR_VALUE lightBufferColorClearValue{};
	lightBufferColorClearValue.Format = lightBufferFormat;
	memcpy(lightBufferColorClearValue.Color, LIGHT_BUFFER_CLEAR_COLOR, sizeof LIGHT_BUFFER_CLEAR_COLOR);

	// create GBuffer root signature and pipeline state
	{
		ComPtr<ID3DBlob> vertexShaderBlob;
		ThrowIfFailed(D3DReadFileToBlob(L"DeferredLightingDemo_GBuffer_VertexShader.cso", &vertexShaderBlob));

		ComPtr<ID3DBlob> pixelShaderBlob;
		ThrowIfFailed(D3DReadFileToBlob(L"DeferredLightingDemo_GBuffer_PixelShader.cso", &pixelShaderBlob));

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
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

		CD3DX12_DESCRIPTOR_RANGE1 texturesDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, PbrMaterial::TotalNumMaps, 0);

		CD3DX12_ROOT_PARAMETER1 rootParameters[GBufferRootParameters::NumRootParameters];
		rootParameters[GBufferRootParameters::MatricesCb].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX);
		rootParameters[GBufferRootParameters::MaterialCb].InitAsConstantBufferView(1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[GBufferRootParameters::TaaBufferCb].InitAsConstantBufferView(1, 1, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX);
		rootParameters[GBufferRootParameters::Textures].InitAsDescriptorTable(1, &texturesDescriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);

		CD3DX12_STATIC_SAMPLER_DESC samplers[] = {
			// linear repeat
			CD3DX12_STATIC_SAMPLER_DESC(0, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR),
		};

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
		rootSignatureDescription.Init_1_1(GBufferRootParameters::NumRootParameters, rootParameters, _countof(samplers), samplers,
			rootSignatureFlags);

		m_GBufferPassRootSignature.SetRootSignatureDesc(rootSignatureDescription.Desc_1_1, featureData.HighestVersion);

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
			CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER Rasterizer;
		} pipelineStateStream;

		D3D12_RT_FORMAT_ARRAY rtvFormats = {};
		rtvFormats.NumRenderTargets = gBufferTexturesCount - 1;

		for (UINT i = 0; i < rtvFormats.NumRenderTargets; ++i)
		{
			rtvFormats.RTFormats[i] = gBufferFormat;
		}

		pipelineStateStream.RootSignature = m_GBufferPassRootSignature.GetRootSignature().Get();

		pipelineStateStream.InputLayout = { VertexAttributes::INPUT_ELEMENTS, VertexAttributes::INPUT_ELEMENT_COUNT };
		pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pipelineStateStream.Vs = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
		pipelineStateStream.Ps = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
		pipelineStateStream.DsvFormat = depthBufferFormat;
		pipelineStateStream.RtvFormats = rtvFormats;
		pipelineStateStream.Rasterizer = CD3DX12_RASTERIZER_DESC(D3D12_FILL_MODE_SOLID, D3D12_CULL_MODE_BACK, FALSE, 0, 0,
			0, TRUE, FALSE, FALSE, 0,
			D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF);

		const D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
			sizeof(PipelineStateStream), &pipelineStateStream
		};

		ThrowIfFailed(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&m_GBufferPassPipelineState)));
	}

	// root signature and pipeline states for light passes
	{
		CD3DX12_STATIC_SAMPLER_DESC lightPassSamplers[] = {
			// GBuffer sampler : point clamp
			CD3DX12_STATIC_SAMPLER_DESC(0, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP),
		};

		// directional light pass
		{
			m_FullScreenMesh = Mesh::CreateBlitTriangle(*commandList);

			ComPtr<ID3DBlob> vertexShaderBlob;
			ThrowIfFailed(D3DReadFileToBlob(L"DeferredLightingDemo_LightBuffer_Directional_VertexShader.cso", &vertexShaderBlob));

			ComPtr<ID3DBlob> pixelShaderBlob;
			ThrowIfFailed(D3DReadFileToBlob(L"DeferredLightingDemo_LightBuffer_Directional_PixelShader.cso", &pixelShaderBlob));

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
				D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

			CD3DX12_DESCRIPTOR_RANGE1 gBufferDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, gBufferTexturesCount, 0);
			CD3DX12_DESCRIPTOR_RANGE1 ambientDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, gBufferTexturesCount);

			CD3DX12_ROOT_PARAMETER1 rootParameters[DirectionalLightBufferRootParameters::NumRootParameters];
			rootParameters[DirectionalLightBufferRootParameters::MatricesCb].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE);
			rootParameters[DirectionalLightBufferRootParameters::DirectionalLightCb].InitAsConstants(sizeof(DirectionalLight) / sizeof(float), 1, 0, D3D12_SHADER_VISIBILITY_PIXEL);
			rootParameters[DirectionalLightBufferRootParameters::ScreenParametersCb].InitAsConstants(sizeof(ScreenParameters) / sizeof(float), 2, 0, D3D12_SHADER_VISIBILITY_PIXEL);
			rootParameters[DirectionalLightBufferRootParameters::GBuffer].InitAsDescriptorTable(1, &gBufferDescriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);
			rootParameters[DirectionalLightBufferRootParameters::Ambient].InitAsDescriptorTable(1, &ambientDescriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);

			CD3DX12_STATIC_SAMPLER_DESC samplers[] = {
				lightPassSamplers[0],
				GetSkyboxSampler(1)
			};

			CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
			rootSignatureDescription.Init_1_1(DirectionalLightBufferRootParameters::NumRootParameters, rootParameters, _countof(samplers), samplers,
				rootSignatureFlags);

			m_DirectionalLightPassRootSignature.SetRootSignatureDesc(rootSignatureDescription.Desc_1_1, featureData.HighestVersion);

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
				CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER Rasterizer;
				CD3DX12_PIPELINE_STATE_STREAM_BLEND_DESC Blend;
				CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL DepthStencil;
			} pipelineStateStream;

			D3D12_RT_FORMAT_ARRAY rtvFormats = {};
			rtvFormats.NumRenderTargets = 1;
			rtvFormats.RTFormats[0] = lightBufferFormat;

			pipelineStateStream.RootSignature = m_DirectionalLightPassRootSignature.GetRootSignature().Get();

			pipelineStateStream.InputLayout = { VertexAttributes::INPUT_ELEMENTS, VertexAttributes::INPUT_ELEMENT_COUNT };
			pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			pipelineStateStream.Vs = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
			pipelineStateStream.Ps = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
			pipelineStateStream.DsvFormat = depthBufferFormat;
			pipelineStateStream.RtvFormats = rtvFormats;
			pipelineStateStream.Rasterizer = CD3DX12_RASTERIZER_DESC(D3D12_FILL_MODE_SOLID, D3D12_CULL_MODE_BACK, FALSE, 0, 0,
				0, TRUE, FALSE, FALSE, 0,
				D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF);
			pipelineStateStream.Blend = AdditiveBlending();

			auto depthStencil = CD3DX12_DEPTH_STENCIL_DESC(CD3DX12_DEFAULT());
			depthStencil.DepthEnable = false;
			depthStencil.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
			pipelineStateStream.DepthStencil = depthStencil;

			const D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
				sizeof(PipelineStateStream), &pipelineStateStream
			};

			ThrowIfFailed(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&m_DirectionalLightPassPipelineState)));
		}

		// light stencil pass
		{
			ComPtr<ID3DBlob> vertexShaderBlob;
			ThrowIfFailed(D3DReadFileToBlob(L"DeferredLightingDemo_LightBuffer_LightStencil_VertexShader.cso", &vertexShaderBlob));

			ComPtr<ID3DBlob> pixelShaderBlob;
			ThrowIfFailed(D3DReadFileToBlob(L"DeferredLightingDemo_LightBuffer_LightStencil_PixelShader.cso", &pixelShaderBlob));

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
				D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS
				;

			CD3DX12_DESCRIPTOR_RANGE1 texturesDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, gBufferTexturesCount, 0);

			CD3DX12_ROOT_PARAMETER1 rootParameters[LightStencilRootParameters::NumRootParameters];
			rootParameters[LightStencilRootParameters::MatricesCb].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX);

			CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
			rootSignatureDescription.Init_1_1(LightStencilRootParameters::NumRootParameters, rootParameters, 0, nullptr,
				rootSignatureFlags);

			m_LightStencilPassRootSignature.SetRootSignatureDesc(rootSignatureDescription.Desc_1_1, featureData.HighestVersion);

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
				CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER Rasterizer;
				CD3DX12_PIPELINE_STATE_STREAM_BLEND_DESC Blend;
				CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL DepthStencil;
			} pipelineStateStream;

			D3D12_RT_FORMAT_ARRAY rtvFormats = {};
			rtvFormats.NumRenderTargets = 0;

			pipelineStateStream.RootSignature = m_LightStencilPassRootSignature.GetRootSignature().Get();

			pipelineStateStream.InputLayout = { VertexAttributes::INPUT_ELEMENTS, VertexAttributes::INPUT_ELEMENT_COUNT };
			pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			pipelineStateStream.Vs = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
			pipelineStateStream.Ps = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
			pipelineStateStream.DsvFormat = depthBufferFormat;
			pipelineStateStream.RtvFormats = rtvFormats;

			// https://ogldev.org/www/tutorial37/tutorial37.html
			// no back-face culling
			pipelineStateStream.Rasterizer = CD3DX12_RASTERIZER_DESC(D3D12_FILL_MODE_SOLID, D3D12_CULL_MODE_NONE, FALSE, 0, 0,
				0, TRUE, FALSE, FALSE, 0,
				D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF);
			pipelineStateStream.Blend = AdditiveBlending();

			auto depthStencil = CD3DX12_DEPTH_STENCIL_DESC(CD3DX12_DEFAULT());
			depthStencil.DepthEnable = true; // read
			depthStencil.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // but not write
			depthStencil.StencilEnable = true;
			depthStencil.StencilWriteMask = UINT8_MAX;

			D3D12_DEPTH_STENCILOP_DESC backFaceStencilOp;
			backFaceStencilOp.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
			backFaceStencilOp.StencilDepthFailOp = D3D12_STENCIL_OP_INCR;
			backFaceStencilOp.StencilPassOp = D3D12_STENCIL_OP_KEEP;
			backFaceStencilOp.StencilFailOp = D3D12_STENCIL_OP_KEEP;
			depthStencil.BackFace = backFaceStencilOp;

			D3D12_DEPTH_STENCILOP_DESC frontFaceStencilOp;
			frontFaceStencilOp.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
			frontFaceStencilOp.StencilDepthFailOp = D3D12_STENCIL_OP_DECR;
			frontFaceStencilOp.StencilPassOp = D3D12_STENCIL_OP_KEEP;
			frontFaceStencilOp.StencilFailOp = D3D12_STENCIL_OP_KEEP;
			depthStencil.FrontFace = frontFaceStencilOp;

			pipelineStateStream.DepthStencil = depthStencil;

			const D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
				sizeof(PipelineStateStream), &pipelineStateStream
			};

			ThrowIfFailed(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&m_LightStencilPassPipelineState)));
		}

		const auto initLightPass = [&lightPassSamplers](ComPtr<ID3D12Device2> device, const std::wstring& vertexShaderPath, const std::wstring& pixelShaderPath,
			RootSignature& rootSignature,
			ComPtr<ID3D12PipelineState>& pipelineState
			)
		{
			ComPtr<ID3DBlob> vertexShaderBlob;
			ThrowIfFailed(D3DReadFileToBlob(vertexShaderPath.c_str(), &vertexShaderBlob));

			ComPtr<ID3DBlob> pixelShaderBlob;
			ThrowIfFailed(D3DReadFileToBlob(pixelShaderPath.c_str(), &pixelShaderBlob));

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
				D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

			CD3DX12_DESCRIPTOR_RANGE1 texturesDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, gBufferTexturesCount, 0);

			CD3DX12_ROOT_PARAMETER1 rootParameters[LightPassRootParameters::NumRootParameters];
			rootParameters[LightPassRootParameters::MatricesCb].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE);
			rootParameters[LightPassRootParameters::LightCb].InitAsConstantBufferView(1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);
			rootParameters[LightPassRootParameters::ScreenParametersCb].InitAsConstants(sizeof(ScreenParameters) / sizeof(float), 2, 0, D3D12_SHADER_VISIBILITY_PIXEL);
			rootParameters[LightPassRootParameters::GBuffer].InitAsDescriptorTable(1, &texturesDescriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);

			CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
			rootSignatureDescription.Init_1_1(LightPassRootParameters::NumRootParameters, rootParameters, _countof(lightPassSamplers), lightPassSamplers,
				rootSignatureFlags);

			rootSignature.SetRootSignatureDesc(rootSignatureDescription.Desc_1_1, featureData.HighestVersion);

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
				CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER Rasterizer;
				CD3DX12_PIPELINE_STATE_STREAM_BLEND_DESC Blend;
				CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL DepthStencil;
			} pipelineStateStream;

			D3D12_RT_FORMAT_ARRAY rtvFormats = {};
			rtvFormats.NumRenderTargets = 1;
			rtvFormats.RTFormats[0] = lightBufferFormat;

			pipelineStateStream.RootSignature = rootSignature.GetRootSignature().Get();

			pipelineStateStream.InputLayout = { VertexAttributes::INPUT_ELEMENTS, VertexAttributes::INPUT_ELEMENT_COUNT };
			pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			pipelineStateStream.Vs = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
			pipelineStateStream.Ps = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
			pipelineStateStream.DsvFormat = depthBufferFormat;
			pipelineStateStream.RtvFormats = rtvFormats;
			pipelineStateStream.Rasterizer = CD3DX12_RASTERIZER_DESC(D3D12_FILL_MODE_SOLID, D3D12_CULL_MODE_FRONT, FALSE, 0, 0,
				0, TRUE, FALSE, FALSE, 0,
				D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF);
			pipelineStateStream.Blend = AdditiveBlending();

			auto depthStencil = CD3DX12_DEPTH_STENCIL_DESC(CD3DX12_DEFAULT());
			depthStencil.DepthEnable = false; // do not read
			depthStencil.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // but not write
			depthStencil.StencilEnable = true;
			depthStencil.StencilReadMask = UINT8_MAX;
			depthStencil.StencilWriteMask = 0;

			D3D12_DEPTH_STENCILOP_DESC stencilOp;
			stencilOp.StencilFunc = D3D12_COMPARISON_FUNC_NOT_EQUAL;
			stencilOp.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
			stencilOp.StencilFailOp = D3D12_STENCIL_OP_KEEP;
			stencilOp.StencilPassOp = D3D12_STENCIL_OP_KEEP;
			depthStencil.FrontFace = stencilOp;
			depthStencil.BackFace = stencilOp;

			pipelineStateStream.DepthStencil = depthStencil;

			const D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
				sizeof(PipelineStateStream), &pipelineStateStream
			};

			ThrowIfFailed(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&pipelineState)));
		};

		// point light pass
		{
			m_PointLightMesh = Mesh::CreateSphere(*commandList, 1.0f);

			initLightPass(device,
				L"DeferredLightingDemo_LightBuffer_Common_VertexShader.cso",
				L"DeferredLightingDemo_LightBuffer_Point_PixelShader.cso",
				m_PointLightPassRootSignature,
				m_PointLightPassPipelineState
			);
		}

		// spot light pass
		{
			m_SpotLightMesh = Mesh::CreateSpotlightPyramid(*commandList, 1, 1);

			initLightPass(device,
				L"DeferredLightingDemo_LightBuffer_Common_VertexShader.cso",
				L"DeferredLightingDemo_LightBuffer_Spot_PixelShader.cso",
				m_SpotLightPassRootSignature,
				m_SpotLightPassPipelineState
			);
		}

		// capsule light pass
		{
			ModelLoader modelLoader;
			m_CapsuleLightMesh = modelLoader.Load(*commandList, "Assets/Models/_builtin/cylinder.obj")->GetMeshes()[0];

			initLightPass(device,
				L"DeferredLightingDemo_LightBuffer_Common_VertexShader.cso",
				L"DeferredLightingDemo_LightBuffer_Capsule_PixelShader.cso",
				m_CapsuleLightPassRootSignature,
				m_CapsuleLightPassPipelineState
			);
		}

		// skybox pass
		{
			m_SkyboxMesh = Mesh::CreateCube(*commandList);

			ComPtr<ID3DBlob> vertexShaderBlob;
			ThrowIfFailed(D3DReadFileToBlob(L"DeferredLightingDemo_Skybox_VertexShader.cso", &vertexShaderBlob));

			ComPtr<ID3DBlob> pixelShaderBlob;
			ThrowIfFailed(D3DReadFileToBlob(L"DeferredLightingDemo_Skybox_PixelShader.cso", &pixelShaderBlob));

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
				D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

			CD3DX12_DESCRIPTOR_RANGE1 texturesDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

			CD3DX12_ROOT_PARAMETER1 rootParameters[SkyboxPassRootParameters::NumRootParameters];
			rootParameters[SkyboxPassRootParameters::MatricesCb].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX);
			rootParameters[SkyboxPassRootParameters::SkyboxCubemap].InitAsDescriptorTable(1, &texturesDescriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);

			D3D12_STATIC_SAMPLER_DESC samplers[] = {
				GetSkyboxSampler(0)
			};

			CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
			rootSignatureDescription.Init_1_1(SkyboxPassRootParameters::NumRootParameters, rootParameters, _countof(samplers), samplers,
				rootSignatureFlags);

			m_SkyboxPassRootSignature.SetRootSignatureDesc(rootSignatureDescription.Desc_1_1, featureData.HighestVersion);

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
				CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER Rasterizer;
				CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL DepthStencil;
			} pipelineStateStream;

			D3D12_RT_FORMAT_ARRAY rtvFormats = {};
			rtvFormats.NumRenderTargets = 1;
			rtvFormats.RTFormats[0] = lightBufferFormat;

			pipelineStateStream.RootSignature = m_SkyboxPassRootSignature.GetRootSignature().Get();

			pipelineStateStream.InputLayout = { VertexAttributes::INPUT_ELEMENTS, VertexAttributes::INPUT_ELEMENT_COUNT };
			pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			pipelineStateStream.Vs = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
			pipelineStateStream.Ps = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
			pipelineStateStream.DsvFormat = depthBufferFormat;
			pipelineStateStream.RtvFormats = rtvFormats;
			pipelineStateStream.Rasterizer = CD3DX12_RASTERIZER_DESC(D3D12_FILL_MODE_SOLID, D3D12_CULL_MODE_FRONT, FALSE, 0, 0,
				0, TRUE, FALSE, FALSE, 0,
				D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF);

			auto depthStencil = CD3DX12_DEPTH_STENCIL_DESC(CD3DX12_DEFAULT());
			depthStencil.DepthEnable = true; // read
			depthStencil.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // but not write
			pipelineStateStream.DepthStencil = depthStencil;

			const D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
				sizeof(PipelineStateStream), &pipelineStateStream
			};

			ThrowIfFailed(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&m_SkyboxPassPipelineState)));
		}

		{
			m_Ssao = std::make_unique<Ssao>(device, *commandList, gBufferFormat, m_Width, m_Height);
		}

		{
			D3D12_SHADER_RESOURCE_VIEW_DESC depthTextureSrv = GetDepthTextureSrv();
			m_Ssr = std::make_unique<Ssr>(lightBufferFormat, depthTextureSrv, m_Width, m_Height);
			m_Ssr->Init(device, *commandList);
		}

		{
			m_AutoExposurePso = std::make_unique<AutoExposure>(device, *commandList);
			m_ToneMappingPso = std::make_unique<ToneMapping>(device, *commandList, resultFormat);
		}

		{
			m_Taa = std::make_unique<Taa>(device, *commandList, resultFormat, m_Width, m_Height);
		}
	}

	// Create lights
	{
		m_DirectionalLight.m_DirectionWs = XMFLOAT4(1.0f, 1.0f, 0.0f, 0.0f);
		m_DirectionalLight.m_Color = XMFLOAT4(0.6f, 0.5f, 0.3f, 0.0f);

		// magenta
		{
			PointLight pointLight(XMFLOAT4(-8, 2, 10, 1));
			pointLight.Color = XMFLOAT4(1.0f, 0.0f, 1.0f, 1.0f);
			m_PointLights.push_back(pointLight);
		}

		// yellow-ish
		{
			PointLight pointLight(XMFLOAT4(-6, 2, 17, 1));
			pointLight.Color = XMFLOAT4(3.0f, 2.0f, 0.25f, 1.0f);
			m_PointLights.push_back(pointLight);
		}

		// cyan-ish
		{
			PointLight pointLight(XMFLOAT4(6, 4, 13, 1), 25.0f);
			pointLight.Color = XMFLOAT4(0.0f, 4.0f, 2.0f, 1.0f);
			m_PointLights.push_back(pointLight);
		}

		// red
		{
			PointLight pointLight(XMFLOAT4(10, 4, 18, 1), 25.0f);
			pointLight.Color = XMFLOAT4(4.0f, 0.0f, 0.1f, 1.0f);
			m_PointLights.push_back(pointLight);
		}

		// green-ish
		{
			PointLight pointLight(XMFLOAT4(-2, 5, 12, 1));
			pointLight.Color = XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f);
			m_PointLights.push_back(pointLight);
		}

		// chest spot light
		{
			SpotLight spotLight;
			spotLight.PositionWs = XMFLOAT4(0.0, 10.0f, 15.0f, 1.0f);
			spotLight.DirectionWs = XMFLOAT4(0.0f, -1.0f, 0.0f, 0.0f);
			spotLight.Color = XMFLOAT4(1.0f, 1.0f, 0.8f, 1);
			spotLight.Intensity = 10.0f;
			spotLight.SpotAngle = XMConvertToRadians(60.0f);
			spotLight.Attenuation = 0.01f;
			m_SpotLights.push_back(spotLight);
		}

		// capsule light (X-axis)
		{
			CapsuleLight capsuleLight;
			capsuleLight.Color = XMFLOAT4(0.0f, 10.0f, 10.0f, 1.0f);
			capsuleLight.PointA = XMFLOAT4(-15.0, 1.0f, 0.0f, 1.0f);
			capsuleLight.PointB = XMFLOAT4(15.0, 1.0f, 0.0f, 1.0f);
			capsuleLight.Attenuation = 0.5f;
			m_CapsuleLights.push_back(capsuleLight);
		}

		// capsule light (Z-axis)
		{
			CapsuleLight capsuleLight;
			capsuleLight.Color = XMFLOAT4(10.0f, 0.0f, 10.0f, 1.0f);
			capsuleLight.PointA = XMFLOAT4(0.0, 1.0f, -15.0f, 1.0f);
			capsuleLight.PointB = XMFLOAT4(0.0, 1.0f, 15.0f, 1.0f);
			capsuleLight.Attenuation = 0.5f;
			m_CapsuleLights.push_back(capsuleLight);
		}
	}

	// Load models
	{
		ModelLoader modelLoader;
		PbrTextureLoader textureLoader(m_WhiteTexture2d);

		{
			auto model = modelLoader.LoadExisting(Mesh::CreatePlane(*commandList));
			auto material = std::make_shared<PbrMaterial>();
			textureLoader.Init(*material);
			material->GetConstants().Metallic = 1.0f;
			material->GetConstants().TilingOffset = { 6, 6, 0, 0 };

			textureLoader.LoadMap(*material, *commandList, PbrMaterial::Diffuse,
					L"Assets/Textures/Ground047/Ground047_1K_Color.jpg");
			textureLoader.LoadMap(*material, *commandList, PbrMaterial::Normal,
					L"Assets/Textures/Ground047/Ground047_1K_NormalDX.jpg");
			textureLoader.LoadMap(*material, *commandList, PbrMaterial::Roughness,
				L"Assets/Textures/Ground047/Ground047_1K_Roughness.jpg");
			textureLoader.LoadMap(*material, *commandList, PbrMaterial::AmbientOcclusion,
				L"Assets/Textures/Ground047/Ground047_1K_AmbientOcclusion.jpg");

			XMMATRIX translationMatrix = XMMatrixTranslation(0.0f, 0.0f, 0.0f);
			XMMATRIX rotationMatrix = XMMatrixIdentity();
			XMMATRIX scaleMatrix = XMMatrixScaling(200.0f, 200.0f, 200.0f);
			XMMATRIX worldMatrix = scaleMatrix * translationMatrix * rotationMatrix;
			m_GameObjects.push_back(GameObject(worldMatrix, model, material));
		}

		{
			auto model = modelLoader.Load(*commandList, "Assets/Models/old-wooden-chest/chest_01.fbx");
			auto material = std::make_shared<PbrMaterial>();
			textureLoader.Init(*material);

			textureLoader.LoadMap(*material, *commandList, PbrMaterial::Diffuse,
				L"Assets/Models/old-wooden-chest/chest_01_BaseColor.png");
			textureLoader.LoadMap(*material, *commandList, PbrMaterial::Normal,
				L"Assets/Models/old-wooden-chest/chest_01_Normal.png");
			textureLoader.LoadMap(*material, *commandList, PbrMaterial::Roughness,
				L"Assets/Models/old-wooden-chest/chest_01_Roughness.png");
			textureLoader.LoadMap(*material, *commandList, PbrMaterial::Metallic,
				L"Assets/Models/old-wooden-chest/chest_01_Metallic.png");

			{
				XMMATRIX translationMatrix = XMMatrixTranslation(0.0f, 0.25f, 15.0f);
				XMMATRIX rotationMatrix = XMMatrixRotationRollPitchYaw(XMConvertToRadians(90), 0, 0);
				XMMATRIX scaleMatrix = XMMatrixScaling(0.01f, 0.01f, 0.01f);
				XMMATRIX worldMatrix = scaleMatrix * rotationMatrix * translationMatrix;
				m_GameObjects.push_back(GameObject(worldMatrix, model, material));
			}
			
			{
				XMMATRIX translationMatrix = XMMatrixTranslation(-50.0f, 0.25f, 15.0f);
				XMMATRIX rotationMatrix = XMMatrixRotationRollPitchYaw(XMConvertToRadians(90), 0, 0);
				XMMATRIX scaleMatrix = XMMatrixScaling(0.01f, 0.01f, 0.01f);
				XMMATRIX worldMatrix = scaleMatrix * rotationMatrix * translationMatrix;
				m_GameObjects.push_back(GameObject(worldMatrix, model, material));
			}
			
		}

		{
			auto model = modelLoader.LoadExisting(Mesh::CreatePlane(*commandList));
			auto material = std::make_shared<PbrMaterial>();
			textureLoader.Init(*material);
			material->GetConstants().Metallic = 1.0f;
			material->GetConstants().Roughness = 0.0f;

			XMMATRIX translationMatrix = XMMatrixTranslation(-50.0f, 0.1f, 15.0f);
			XMMATRIX rotationMatrix = XMMatrixIdentity();
			XMMATRIX scaleMatrix = XMMatrixScaling(30.0f, 30.0f, 30.0f);
			XMMATRIX worldMatrix = scaleMatrix * translationMatrix * rotationMatrix;
			m_GameObjects.push_back(GameObject(worldMatrix, model, material));
		}

		{
			auto model = modelLoader.Load(*commandList, "Assets/Models/cerberus/Cerberus_LP.FBX");
			auto material = std::make_shared<PbrMaterial>();
			textureLoader.Init(*material);

			textureLoader.LoadMap(*material, *commandList, PbrMaterial::Diffuse,
				L"Assets/Models/cerberus/Cerberus_A.jpg");
			textureLoader.LoadMap(*material, *commandList, PbrMaterial::Normal,
				L"Assets/Models/cerberus/Cerberus_N.jpg");
			textureLoader.LoadMap(*material, *commandList, PbrMaterial::Roughness,
				L"Assets/Models/cerberus/Cerberus_R.jpg");
			textureLoader.LoadMap(*material, *commandList, PbrMaterial::Metallic,
				L"Assets/Models/cerberus/Cerberus_M.jpg");

			XMMATRIX translationMatrix = XMMatrixTranslation(15.0f, 5.0f, 10.0f);
			XMMATRIX rotationMatrix = XMMatrixRotationRollPitchYaw(XMConvertToRadians(90), XMConvertToRadians(135), XMConvertToRadians(0));
			XMMATRIX scaleMatrix = XMMatrixScaling(0.10f, 0.10f, 0.10f);
			XMMATRIX worldMatrix = scaleMatrix * rotationMatrix * translationMatrix;
			m_GameObjects.push_back(GameObject(worldMatrix, model, material));
		}

		{
			const int steps = 5;
			for (auto x = 0; x < steps; ++x)
			{
				for (auto y = 0; y < steps; ++y)
				{
					auto model = modelLoader.LoadExisting(Mesh::CreateSphere(*commandList));
					auto material = std::make_shared<PbrMaterial>();
					textureLoader.Init(*material);
					material->GetConstants().Metallic = static_cast<float>(x) / (steps - 1);
					material->GetConstants().Roughness = static_cast<float>(y) / (steps - 1);
					
					XMMATRIX translationMatrix = XMMatrixTranslation(x * 1.5f, 5.0f + y * 2.0f, 25.0f);
					XMMATRIX worldMatrix = translationMatrix;
					m_GameObjects.push_back(GameObject(worldMatrix, model, material));
				}
			}
		}

		{
			for (const auto&pointLight : m_PointLights)
			{
				auto mesh = modelLoader.LoadExisting(Mesh::CreateSphere(*commandList, 0.5f));
				auto material = std::make_shared<PbrMaterial>();
				textureLoader.Init(*material);

				material->GetConstants().Metallic = 0.2f;
				material->GetConstants().Roughness = 0.25f;
				material->GetConstants().Diffuse = NormalizeColor(pointLight.Color);
				material->GetConstants().Emission = GetColorMagnitude(pointLight.Color);

				auto positionWS = XMLoadFloat4(&pointLight.PositionWs);
				auto modelMatrix = XMMatrixTranslationFromVector(positionWS);
				m_GameObjects.push_back(GameObject(modelMatrix, mesh, material));
			}
		}

		commandList->LoadTextureFromFile(m_Skybox, L"Assets/Textures/skybox/skybox.dds", TextureUsageType::Albedo);
	}

	{
		PIXScope(*commandList, "Diffuse Irradiance Convolution");

		const uint32_t arraySize = Cubemap::SIDES_COUNT;
		D3D12_RESOURCE_DESC skyboxDesc = m_Skybox.GetD3D12ResourceDesc();

		const UINT diffuseIrradianceMapSize = 128;
		const auto diffuseIrradianceMapDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			DXGI_FORMAT_R16G16B16A16_FLOAT,
			diffuseIrradianceMapSize, diffuseIrradianceMapSize,
			arraySize, 1,
			1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

		auto diffuseIrradianceMap = Texture(diffuseIrradianceMapDesc, nullptr,
			TextureUsageType::Albedo,
			L"Skybox IBL (Diffuse Irradiance)");
		m_DiffuseIrradianceMapRt.AttachTexture(Color0, diffuseIrradianceMap);

		DiffuseIrradiance diffuseIrradiancePso(device, *commandList, diffuseIrradianceMapDesc.Format);
		diffuseIrradiancePso.SetContext(*commandList);
		diffuseIrradiancePso.SetSourceCubemap(*commandList, m_Skybox);

		for (UINT32 sideIndex = 0; sideIndex < Cubemap::SIDES_COUNT; ++sideIndex)
		{
			diffuseIrradiancePso.SetRenderTarget(*commandList, m_DiffuseIrradianceMapRt, sideIndex);
			diffuseIrradiancePso.Draw(*commandList, sideIndex);
		}
	}

	{
		PIXScope(*commandList, "BRDF Integration");

		const uint32_t arraySize = Cubemap::SIDES_COUNT;
		D3D12_RESOURCE_DESC skyboxDesc = m_Skybox.GetD3D12ResourceDesc();

		const UINT brdfIntegrationMapSize = 512;
		const auto diffuseIrradianceMapDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			DXGI_FORMAT_R16G16_FLOAT,
			brdfIntegrationMapSize, brdfIntegrationMapSize,
			arraySize, 1,
			1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

		auto brdfIntegrationMap = Texture(diffuseIrradianceMapDesc, nullptr,
			TextureUsageType::Other,
			L"BRDF Integration Map");
		m_BrdfIntegrationMapRt.AttachTexture(Color0, brdfIntegrationMap);

		auto device = Application::Get().GetDevice();
		BrdfIntegration brdfIntegrationPso(device, *commandList, diffuseIrradianceMapDesc.Format);
		brdfIntegrationPso.SetContext(*commandList);
		brdfIntegrationPso.SetRenderTarget(*commandList, m_BrdfIntegrationMapRt);
		brdfIntegrationPso.Draw(*commandList);
	}

	{
		PIXScope(*commandList, "Pre-Filter Environment");

		const uint32_t arraySize = Cubemap::SIDES_COUNT;
		D3D12_RESOURCE_DESC skyboxDesc = m_Skybox.GetD3D12ResourceDesc();

		const UINT mipLevels = 5;

		const auto preFilterEnvironmentMapDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			DXGI_FORMAT_R16G16B16A16_FLOAT,
			skyboxDesc.Width, skyboxDesc.Height,
			arraySize, mipLevels,
			1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

		auto preFilterEnvironmentMap = Texture(preFilterEnvironmentMapDesc, nullptr,
			TextureUsageType::Albedo,
			L"Skybox IBL (Pre-Filtered Environment Map)");
		m_PreFilterEnvironmentMapRt.AttachTexture(Color0, preFilterEnvironmentMap);

		PreFilterEnvironment preFilterEnvironmentPso(device, *commandList, preFilterEnvironmentMapDesc.Format);
		preFilterEnvironmentPso.SetContext(*commandList);
		preFilterEnvironmentPso.SetSourceCubemap(*commandList, m_Skybox);

		for (UINT mipLevel = 0; mipLevel < mipLevels; ++mipLevel)
		{
			PIXScope(*commandList, ("Pre-Filter Environment: Mip " + std::to_string(mipLevel)).c_str());

			float roughness = static_cast<float>(mipLevel) / static_cast<float>(mipLevels - 1);

			for (UINT32 sideIndex = 0; sideIndex < Cubemap::SIDES_COUNT; ++sideIndex)
			{
				preFilterEnvironmentPso.SetRenderTarget(*commandList, m_PreFilterEnvironmentMapRt, sideIndex, mipLevel);
				preFilterEnvironmentPso.Draw(*commandList, roughness, sideIndex);
			}
		}
	}

	Texture depthBuffer;

	// Depth Stencil
	{
		auto depthDesc = CD3DX12_RESOURCE_DESC::Tex2D(depthBufferFormat,
			m_Width, m_Height,
			1, 1,
			1, 0,
			D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
		D3D12_CLEAR_VALUE depthClearValue;
		depthClearValue.Format = depthDesc.Format;
		depthClearValue.DepthStencil = { 1.0f, 0 };

		depthBuffer = Texture(depthDesc, &depthClearValue,
			TextureUsageType::Depth,
			L"Depth-Stencil");
		m_DepthTexture = Texture(depthDesc, &depthClearValue,
			TextureUsageType::Depth,
			L"GBuffer-Depth");
	}

	// GBuffer Render Target
	{
		auto colorDesc = CD3DX12_RESOURCE_DESC::Tex2D(gBufferFormat,
			m_Width, m_Height,
			1, 1,
			1, 0,
			D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

		auto diffuseTexture = Texture(colorDesc, &gBufferColorClearValue,
			TextureUsageType::RenderTarget,
			L"GBuffer-Diffuse");

		auto normalTexture = Texture(colorDesc, &gBufferColorClearValue,
			TextureUsageType::RenderTarget,
			L"GBuffer-Normal");

		auto surfaceTexture = Texture(colorDesc, &gBufferColorClearValue,
			TextureUsageType::RenderTarget,
			L"GBuffer-Surface");

		auto velocityTexture = Texture(colorDesc, &velocityColorClearValue,
			TextureUsageType::RenderTarget,
			L"GBuffer-Velocity");

		m_GBufferRenderTarget.AttachTexture(GetGBufferTextureAttachmentPoint(GBufferTextureType::Diffuse), diffuseTexture);
		m_GBufferRenderTarget.AttachTexture(GetGBufferTextureAttachmentPoint(GBufferTextureType::Normals), normalTexture);
		m_GBufferRenderTarget.AttachTexture(GetGBufferTextureAttachmentPoint(GBufferTextureType::Surface), surfaceTexture);
		m_GBufferRenderTarget.AttachTexture(GetGBufferTextureAttachmentPoint(GBufferTextureType::Velocity), velocityTexture);
		m_GBufferRenderTarget.AttachTexture(GetGBufferTextureAttachmentPoint(GBufferTextureType::DepthStencil), depthBuffer);

		m_SurfaceRenderTarget.AttachTexture(Color0, GetGBufferTexture(GBufferTextureType::Surface));
	}

	// Light Buffer
	{
		auto colorDesc = CD3DX12_RESOURCE_DESC::Tex2D(lightBufferFormat,
			m_Width, m_Height,
			1, 1,
			1, 0,
			D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

		auto lightBuffer = Texture(colorDesc, &lightBufferColorClearValue,
			TextureUsageType::RenderTarget,
			L"Light Buffer");

		m_LightBufferRenderTarget.AttachTexture(Color0, lightBuffer);
		m_LightBufferRenderTarget.AttachTexture(DepthStencil, depthBuffer);

		m_LightStencilRenderTarget.AttachTexture(DepthStencil, depthBuffer);
	}

	// Result
	{
		auto colorDesc = CD3DX12_RESOURCE_DESC::Tex2D(resultFormat,
			m_Width, m_Height,
			1, 1,
			1, 0,
			D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

		auto resultRT = Texture(colorDesc, nullptr,
			TextureUsageType::RenderTarget,
			L"Result RT");

		m_ResultRenderTarget.AttachTexture(Color0, resultRT);
	}

	auto fenceValue = commandQueue->ExecuteCommandList(commandList);
	commandQueue->WaitForFenceValue(fenceValue);

	return true;
}

void DeferredLightingDemo::OnResize(ResizeEventArgs& e)
{
	Base::OnResize(e);

	if (m_Width != e.Width || m_Height != e.Height)
	{
		m_Width = std::max(1, e.Width);
		m_Height = std::max(1, e.Height);

		const float aspectRatio = static_cast<float>(m_Width) / static_cast<float>(m_Height);
		m_Camera.SetProjection(45.0f, aspectRatio, 0.1f, 1000.0f);

		m_Viewport = CD3DX12_VIEWPORT(0.0f, 0.0f,
			static_cast<float>(m_Width), static_cast<float>(m_Height));

		m_GBufferRenderTarget.Resize(m_Width, m_Height);
		m_LightBufferRenderTarget.Resize(m_Width, m_Height);
		m_LightBufferRenderTarget.AttachTexture(DepthStencil, m_GBufferRenderTarget.GetTexture(DepthStencil));
		m_LightStencilRenderTarget.AttachTexture(DepthStencil, m_GBufferRenderTarget.GetTexture(DepthStencil));
		m_DepthTexture.Resize(m_Width, m_Height);
		m_ResultRenderTarget.Resize(m_Width, m_Height);

		if (m_Ssao != nullptr)
		{
			m_Ssao->Resize(m_Width, m_Height);
		}

		m_SurfaceRenderTarget.AttachTexture(Color0, GetGBufferTexture(GBufferTextureType::Surface));

		if (m_Ssr != nullptr)
		{
			m_Ssr->Resize(m_Width, m_Height);
		}

		if (m_Taa != nullptr)
		{
			m_Taa->Resize(m_Width, m_Height);
		}
	}
}

void DeferredLightingDemo::UnloadContent()
{
}

void DeferredLightingDemo::OnUpdate(UpdateEventArgs& e)
{
	static uint64_t frameCount = 0;
	static double totalTime = 0.0;

	Base::OnUpdate(e);

	if (e.ElapsedTime > 1.0f) return;

	totalTime += e.ElapsedTime;
	m_Time += e.ElapsedTime;
	frameCount++;

	if (totalTime > 1.0)
	{
		double fps = frameCount / totalTime;

		char buffer[512];
		sprintf_s(buffer, "FPS: %f\n", fps);
		OutputDebugStringA(buffer);

		frameCount = 0;
		totalTime = 0.0;
	}

	// Update the camera.
	float speedMultiplier = (m_CameraController.m_Shift ? 16.0f : 4.0f);

	XMVECTOR cameraTranslate = XMVectorSet(m_CameraController.m_Right - m_CameraController.m_Left, 0.0f,
		m_CameraController.m_Forward - m_CameraController.m_Backward,
		1.0f) * speedMultiplier
		* static_cast<float>(e.ElapsedTime);
	XMVECTOR cameraPan = XMVectorSet(0.0f, m_CameraController.m_Up - m_CameraController.m_Down, 0.0f, 1.0f) *
		speedMultiplier *
		static_cast<float>(e.ElapsedTime);
	m_Camera.Translate(cameraTranslate, Space::Local);
	m_Camera.Translate(cameraPan, Space::Local);

	XMVECTOR cameraRotation = XMQuaternionRotationRollPitchYaw(XMConvertToRadians(m_CameraController.m_Pitch),
		XMConvertToRadians(m_CameraController.m_Yaw), 0.0f);
	m_Camera.SetRotation(cameraRotation);

	auto dt = static_cast<float>(e.ElapsedTime);
	m_DeltaTime = dt;

	if (m_AnimateLights)
	{
		XMVECTOR dirLightDirectionWs = XMLoadFloat4(&m_DirectionalLight.m_DirectionWs);
		dirLightDirectionWs = XMVector4Transform(dirLightDirectionWs,
			XMMatrixRotationAxis(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f),
				XMConvertToRadians(90.0f * dt)));
		XMStoreFloat4(&m_DirectionalLight.m_DirectionWs, dirLightDirectionWs);
	}
}

void DeferredLightingDemo::OnRender(RenderEventArgs& e)
{
	Base::OnRender(e);

	const auto commandQueue = Application::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	const auto commandList = commandQueue->GetCommandList();

	{
		PIXScope(*commandList, "Clear Render Targets");

		{
			PIXScope(*commandList, "Clear GBuffer");
			commandList->ClearRenderTarget(m_GBufferRenderTarget, CLEAR_COLOR, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL);
			commandList->ClearTexture(GetGBufferTexture(GBufferTextureType::Velocity), VELOCITY_CLEAR_COLOR);
		}

		{
			PIXScope(*commandList, "Clear Light Buffer");
			commandList->ClearTexture(m_LightBufferRenderTarget.GetTexture(Color0), LIGHT_BUFFER_CLEAR_COLOR);
		}
	}

	commandList->SetViewport(m_Viewport);
	commandList->SetScissorRect(m_ScissorRect);

	const XMMATRIX viewMatrix = m_Camera.GetViewMatrix();
	const XMMATRIX projectionMatrix = m_Camera.GetProjectionMatrix();
	const XMMATRIX viewProjectionMatrix = viewMatrix * projectionMatrix;

	{
		PIXScope(*commandList, "GBuffer Pass");

		commandList->SetRenderTarget(m_GBufferRenderTarget);
		commandList->SetGraphicsRootSignature(m_GBufferPassRootSignature);
		commandList->SetPipelineState(m_GBufferPassPipelineState);


		TaaCBuffer taaBuffer;
		taaBuffer.JitterOffset = m_TaaEnabled ? m_Taa->ComputeJitterOffset(m_Width, m_Height) : XMFLOAT2(0.0f, 0.0f);

		for (const auto& go : m_GameObjects)
		{
			MatricesCb matricesCb;
			matricesCb.Compute(go.GetWorldMatrix(), viewMatrix, viewProjectionMatrix, projectionMatrix);
			commandList->SetGraphicsDynamicConstantBuffer(GBufferRootParameters::MatricesCb, matricesCb);

			auto material = go.GetMaterial<PbrMaterial>();
			material->SetDynamicConstantBuffer(*commandList, GBufferRootParameters::MaterialCb);
			material->SetShaderResourceViews(*commandList, GBufferRootParameters::Textures);

			taaBuffer.PreviousModelViewProjectionMatrix = go.GetPreviousWorldMatrix() * m_Taa->GetPreviousViewProjectionMatrix();
			commandList->SetGraphicsDynamicConstantBuffer(GBufferRootParameters::TaaBufferCb, taaBuffer);

			const auto& model = go.GetModel();
			for (const auto& mesh : model->GetMeshes())
			{
				mesh->Draw(*commandList);
			}
		}
	}

	{
		PIXScope(*commandList, "Copy Depth Buffer to Depth Texture");

		commandList->CopyResource(m_DepthTexture, m_GBufferRenderTarget.GetTexture(DepthStencil));
	}

	if (m_SsaoEnabled)
	{
		PIXScope(*commandList, "SSAO");

		D3D12_SHADER_RESOURCE_VIEW_DESC depthTextureSrv = GetDepthTextureSrv();
		constexpr float radius = 0.25f;
		constexpr float power = 2.0f;
		m_Ssao->SsaoPass(*commandList, GetGBufferTexture(GBufferTextureType::Normals), m_DepthTexture, viewMatrix, projectionMatrix, &depthTextureSrv, radius, power);
		m_Ssao->BlurPass(*commandList, m_SurfaceRenderTarget);
	}

	{
		m_Ssr->SetMatrices(viewMatrix, projectionMatrix);
		const auto jitterOffset = m_Taa->GetCurrentJitterOffset();
		m_Ssr->SetJitterOffset(jitterOffset);

		const Texture& normals = GetGBufferTexture(GBufferTextureType::Normals);
		const Texture& surface = GetGBufferTexture(GBufferTextureType::Surface);
		const Texture& depth = m_DepthTexture;
		const RenderTarget& resultRenderTarget = m_LightBufferRenderTarget;
		m_Ssr->Execute(*commandList, normals, surface, depth, resultRenderTarget);
	}

	commandList->SetViewport(m_Viewport);
	commandList->SetScissorRect(m_ScissorRect);

	{
		PIXScope(*commandList, "Light Passes");

		commandList->SetRenderTarget(m_LightBufferRenderTarget);
		ScreenParameters screenParameters;
		screenParameters.Width = static_cast<float>(m_Width);
		screenParameters.Height = static_cast<float>(m_Height);
		screenParameters.OneOverWidth = 1.0f / m_Width;
		screenParameters.OneOverHeight = 1.0f / m_Height;

		{
			PIXScope(*commandList, "Directional Light Pass");

			commandList->SetGraphicsRootSignature(m_DirectionalLightPassRootSignature);
			commandList->SetPipelineState(m_DirectionalLightPassPipelineState);

			MatricesCb matricesCb;
			matricesCb.Compute(XMMatrixIdentity(), viewMatrix, viewProjectionMatrix, projectionMatrix);

			commandList->SetGraphicsDynamicConstantBuffer(DirectionalLightBufferRootParameters::MatricesCb, matricesCb);
			commandList->SetGraphics32BitConstants(DirectionalLightBufferRootParameters::DirectionalLightCb, m_DirectionalLight);
			commandList->SetGraphics32BitConstants(DirectionalLightBufferRootParameters::ScreenParametersCb, screenParameters);
			BindGBufferAsSRV(*commandList, DirectionalLightBufferRootParameters::GBuffer);


			const auto& diffuseIrradianceMap = m_DiffuseIrradianceMapRt.GetTexture(Color0);
			D3D12_SHADER_RESOURCE_VIEW_DESC irradianceSrvDesc;
			irradianceSrvDesc.Format = diffuseIrradianceMap.GetD3D12ResourceDesc().Format;
			irradianceSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			irradianceSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
			irradianceSrvDesc.TextureCube.MipLevels = 1;
			irradianceSrvDesc.TextureCube.MostDetailedMip = 0;
			irradianceSrvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
			commandList->SetShaderResourceView(DirectionalLightBufferRootParameters::Ambient, 0, diffuseIrradianceMap, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0, UINT_MAX, &irradianceSrvDesc);

			const auto& preFilterEnvironmentMap = m_PreFilterEnvironmentMapRt.GetTexture(Color0);
			D3D12_SHADER_RESOURCE_VIEW_DESC preFilterEnvironmentSrvDesc;
			preFilterEnvironmentSrvDesc.Format = diffuseIrradianceMap.GetD3D12ResourceDesc().Format;
			preFilterEnvironmentSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			preFilterEnvironmentSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
			preFilterEnvironmentSrvDesc.TextureCube.MipLevels = -1;
			preFilterEnvironmentSrvDesc.TextureCube.MostDetailedMip = 0;
			preFilterEnvironmentSrvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
			commandList->SetShaderResourceView(DirectionalLightBufferRootParameters::Ambient, 1, preFilterEnvironmentMap, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0, UINT_MAX, &preFilterEnvironmentSrvDesc);

			commandList->SetShaderResourceView(DirectionalLightBufferRootParameters::Ambient, 2, m_BrdfIntegrationMapRt.GetTexture(Color0), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

			m_FullScreenMesh->Draw(*commandList);
		}

		commandList->SetStencilRef(0);

		{
			PIXScope(*commandList, "Point Light Pass");

			for (const auto& pointLight : m_PointLights)
			{
				MatricesCb matricesCb;
				XMMATRIX modelMatrix = GetModelMatrix(pointLight);
				matricesCb.Compute(modelMatrix, viewMatrix, viewProjectionMatrix, projectionMatrix);

				const auto mesh = m_PointLightMesh;
				LightStencilPass(*commandList, matricesCb, mesh);
				PointLightPass(*commandList, matricesCb, pointLight, screenParameters, mesh);
			}
		}

		{
			PIXScope(*commandList, "Spot Light Pass");

			for (const auto& spotLight : m_SpotLights)
			{
				MatricesCb matricesCb;
				XMMATRIX modelMatrix = GetModelMatrix(spotLight);
				matricesCb.Compute(modelMatrix, viewMatrix, viewProjectionMatrix, projectionMatrix);

				const auto mesh = m_SpotLightMesh;
				LightStencilPass(*commandList, matricesCb, mesh);
				SpotLightPass(*commandList, matricesCb, spotLight, screenParameters, mesh);
			}
		}

		{
			PIXScope(*commandList, "Capsule Light Pass");

			for (const auto& capsuleLight : m_CapsuleLights)
			{
				MatricesCb matricesCb;
				XMMATRIX modelMatrix = GetModelMatrix(capsuleLight);
				matricesCb.Compute(modelMatrix, viewMatrix, viewProjectionMatrix, projectionMatrix);

				const auto mesh = m_CapsuleLightMesh;
				LightStencilPass(*commandList, matricesCb, mesh);
				CapsuleLightPass(*commandList, matricesCb, capsuleLight, screenParameters, mesh);
			}
		}
	}

	{
		PIXScope(*commandList, "Skybox Pass");

		commandList->SetGraphicsRootSignature(m_SkyboxPassRootSignature);
		commandList->SetPipelineState(m_SkyboxPassPipelineState);

		MatricesCb matricesCb;
		const XMMATRIX modelMatrix = XMMatrixScaling(2, 2, 2) * XMMatrixTranslationFromVector(m_Camera.GetTranslation());
		matricesCb.Compute(modelMatrix, viewMatrix, viewProjectionMatrix, projectionMatrix);
		commandList->SetGraphicsDynamicConstantBuffer(SkyboxPassRootParameters::MatricesCb, matricesCb);

		const auto& skybox = m_Skybox;

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
		srvDesc.Format = skybox.GetD3D12ResourceDesc().Format;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.TextureCube.MipLevels = 1;
		srvDesc.TextureCube.MostDetailedMip = 0;
		srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;

		commandList->SetShaderResourceView(SkyboxPassRootParameters::SkyboxCubemap, 0, skybox,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			0, UINT_MAX, &srvDesc
		);

		m_SkyboxMesh->Draw(*commandList);
	}

	{
		m_Ssr->CaptureSceneColor(*commandList, m_LightBufferRenderTarget.GetTexture(Color0));
	}

	{
		m_AutoExposurePso->Dispatch(*commandList, m_LightBufferRenderTarget.GetTexture(Color0), m_DeltaTime);
	}

	{
		auto& source = m_LightBufferRenderTarget.GetTexture(Color0);
		m_ToneMappingPso->Blit(*commandList, source, m_AutoExposurePso->GetLuminanceOutput(), m_ResultRenderTarget);
	}

	if (m_TaaEnabled)
	{
		const Texture& currentBuffer = m_ResultRenderTarget.GetTexture(Color0);
		m_Taa->Resolve(*commandList, currentBuffer, GetGBufferTexture(GBufferTextureType::Velocity));
	}

	// Maintain TAA data
	if (m_TaaEnabled)
	{
		for (auto& go : m_GameObjects)
		{
			go.OnRenderedFrame();
		}

		m_Taa->OnRenderedFrame(viewProjectionMatrix);
	}

	commandQueue->ExecuteCommandList(commandList);
	const auto& presentedTexture = m_TaaEnabled ? m_Taa->GetResolvedTexture() : m_ResultRenderTarget.GetTexture(Color0);
	PWindow->Present(presentedTexture);
}

void DeferredLightingDemo::BindGBufferAsSRV(CommandList& commandList, uint32_t rootParameterIndex)
{
	commandList.SetShaderResourceView(rootParameterIndex, 0, GetGBufferTexture(GBufferTextureType::Diffuse), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	commandList.SetShaderResourceView(rootParameterIndex, 1, GetGBufferTexture(GBufferTextureType::Normals), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	commandList.SetShaderResourceView(rootParameterIndex, 2, GetGBufferTexture(GBufferTextureType::Surface), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	auto depthStencilSrvDesc = GetDepthTextureSrv();
	commandList.SetShaderResourceView(rootParameterIndex, 3, m_DepthTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0, 1, &depthStencilSrvDesc);
}

AttachmentPoint DeferredLightingDemo::GetGBufferTextureAttachmentPoint(GBufferTextureType type)
{
	switch (type)
	{
	case GBufferTextureType::Diffuse:
		return Color0;
	case GBufferTextureType::Normals:
		return Color1;
	case GBufferTextureType::Surface:
		return Color2;
	case GBufferTextureType::Velocity:
		return Color3;
	case GBufferTextureType::DepthStencil:
		return DepthStencil;
	default:
		throw std::exception("Invalid GBuffer texture type.");
	}
}

const Texture& DeferredLightingDemo::GetGBufferTexture(GBufferTextureType type)
{
	return m_GBufferRenderTarget.GetTexture(GetGBufferTextureAttachmentPoint(type));
}

void DeferredLightingDemo::LightStencilPass(CommandList& commandList, const MatricesCb& matricesCb, std::shared_ptr<Mesh> mesh)
{
	PIXScope(commandList, "Light Stencil Pass");

	commandList.SetRenderTarget(m_LightStencilRenderTarget);
	commandList.ClearDepthStencilTexture(m_LightStencilRenderTarget.GetTexture(DepthStencil), D3D12_CLEAR_FLAG_STENCIL);

	commandList.SetGraphicsRootSignature(m_LightStencilPassRootSignature);
	commandList.SetPipelineState(m_LightStencilPassPipelineState);

	commandList.SetGraphicsDynamicConstantBuffer(LightStencilRootParameters::MatricesCb, matricesCb);

	mesh->Draw(commandList);
}

D3D12_SHADER_RESOURCE_VIEW_DESC DeferredLightingDemo::GetDepthTextureSrv() const
{
	D3D12_SHADER_RESOURCE_VIEW_DESC depthStencilSrvDesc;
	depthStencilSrvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	depthStencilSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	depthStencilSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	depthStencilSrvDesc.Texture2D.MipLevels = 1;
	depthStencilSrvDesc.Texture2D.MostDetailedMip = 0;
	depthStencilSrvDesc.Texture2D.PlaneSlice = 0;
	depthStencilSrvDesc.Texture2D.ResourceMinLODClamp = 0.0;
	return depthStencilSrvDesc;
}

void DeferredLightingDemo::PointLightPass(CommandList& commandList, const MatricesCb& matricesCb, const PointLight& pointLight, const ScreenParameters& screenParameters, std::shared_ptr<Mesh> mesh)
{
	commandList.SetRenderTarget(m_LightBufferRenderTarget);

	commandList.SetGraphicsRootSignature(m_PointLightPassRootSignature);
	commandList.SetPipelineState(m_PointLightPassPipelineState);

	commandList.SetGraphicsDynamicConstantBuffer(LightPassRootParameters::MatricesCb, matricesCb);
	commandList.SetGraphicsDynamicConstantBuffer(LightPassRootParameters::LightCb, pointLight);
	commandList.SetGraphics32BitConstants(LightPassRootParameters::ScreenParametersCb, screenParameters);

	BindGBufferAsSRV(commandList, LightPassRootParameters::GBuffer);

	mesh->Draw(commandList);
}

void DeferredLightingDemo::SpotLightPass(CommandList& commandList, const MatricesCb& matricesCb, const SpotLight& spotLight, const ScreenParameters& screenParameters, const std::shared_ptr<Mesh> mesh)
{
	commandList.SetRenderTarget(m_LightBufferRenderTarget);

	commandList.SetGraphicsRootSignature(m_SpotLightPassRootSignature);
	commandList.SetPipelineState(m_SpotLightPassPipelineState);

	commandList.SetGraphicsDynamicConstantBuffer(LightPassRootParameters::MatricesCb, matricesCb);
	commandList.SetGraphicsDynamicConstantBuffer(LightPassRootParameters::LightCb, spotLight);
	commandList.SetGraphics32BitConstants(LightPassRootParameters::ScreenParametersCb, screenParameters);

	BindGBufferAsSRV(commandList, LightPassRootParameters::GBuffer);

	mesh->Draw(commandList);
}

void DeferredLightingDemo::CapsuleLightPass(CommandList& commandList, const MatricesCb& matricesCb, const CapsuleLight& capsuleLight, const ScreenParameters& screenParameters, const std::shared_ptr<Mesh> mesh)
{
	commandList.SetRenderTarget(m_LightBufferRenderTarget);

	commandList.SetGraphicsRootSignature(m_CapsuleLightPassRootSignature);
	commandList.SetPipelineState(m_CapsuleLightPassPipelineState);

	commandList.SetGraphicsDynamicConstantBuffer(LightPassRootParameters::MatricesCb, matricesCb);
	commandList.SetGraphicsDynamicConstantBuffer(LightPassRootParameters::LightCb, capsuleLight);
	commandList.SetGraphics32BitConstants(LightPassRootParameters::ScreenParametersCb, screenParameters);

	BindGBufferAsSRV(commandList, LightPassRootParameters::GBuffer);

	mesh->Draw(commandList);
}

void DeferredLightingDemo::OnKeyPressed(KeyEventArgs& e)
{
	Base::OnKeyPressed(e);

	switch (e.Key)
	{
	case KeyCode::Escape:
		Application::Get().Quit(0);
		break;
	case KeyCode::Enter:
		if (e.Alt)
		{
	case KeyCode::F11:
		if (allowFullscreenToggle)
		{
			PWindow->ToggleFullscreen();
			allowFullscreenToggle = false;
		}
		break;
		}
	case KeyCode::V:
		PWindow->ToggleVSync();
		m_GraphicsSettings.VSync = !m_GraphicsSettings.VSync;
		break;
	case KeyCode::R:
		// Reset camera transform
		m_Camera.SetTranslation(m_PAlignedCameraData->m_InitialPosition);
		m_Camera.SetRotation(m_PAlignedCameraData->m_InitialQRotation);
		m_CameraController.m_Pitch = 0.0f;
		m_CameraController.m_Yaw = 0.0f;
		break;
	case KeyCode::Up:
	case KeyCode::W:
		m_CameraController.m_Forward = 1.0f;
		break;
	case KeyCode::Left:
	case KeyCode::A:
		m_CameraController.m_Left = 1.0f;
		break;
	case KeyCode::Down:
	case KeyCode::S:
		m_CameraController.m_Backward = 1.0f;
		break;
	case KeyCode::Right:
	case KeyCode::D:
		m_CameraController.m_Right = 1.0f;
		break;
	case KeyCode::Q:
		m_CameraController.m_Down = 1.0f;
		break;
	case KeyCode::E:
		m_CameraController.m_Up = 1.0f;
		break;
	case KeyCode::L:
		m_AnimateLights = !m_AnimateLights;
		break;
	case KeyCode::ShiftKey:
		m_CameraController.m_Shift = true;
		break;
	}
}

void DeferredLightingDemo::OnKeyReleased(KeyEventArgs& e)
{
	Base::OnKeyReleased(e);

	switch (e.Key)
	{
	case KeyCode::Enter:
		if (e.Alt)
		{
	case KeyCode::F11:
		allowFullscreenToggle = true;
		}
		break;
	case KeyCode::Up:
	case KeyCode::W:
		m_CameraController.m_Forward = 0.0f;
		break;
	case KeyCode::Left:
	case KeyCode::A:
		m_CameraController.m_Left = 0.0f;
		break;
	case KeyCode::Down:
	case KeyCode::S:
		m_CameraController.m_Backward = 0.0f;
		break;
	case KeyCode::Right:
	case KeyCode::D:
		m_CameraController.m_Right = 0.0f;
		break;
	case KeyCode::Q:
		m_CameraController.m_Down = 0.0f;
		break;
	case KeyCode::E:
		m_CameraController.m_Up = 0.0f;
		break;
	case KeyCode::ShiftKey:
		m_CameraController.m_Shift = false;
		break;
	case KeyCode::O:
		m_SsaoEnabled = !m_SsaoEnabled;
		OutputDebugStringA(m_SsaoEnabled ? "SSAO: On\n" : "SSAO: Off\n");
		break;
	case KeyCode::T:
		m_TaaEnabled = !m_TaaEnabled;
		OutputDebugStringA(m_TaaEnabled ? "TAA: On\n" : "TAA: Off\n");
		break;
	}
}

void DeferredLightingDemo::OnMouseMoved(MouseMotionEventArgs& e)
{
	Base::OnMouseMoved(e);

	const float mouseSpeed = 0.1f;

	if (e.LeftButton)
	{
		m_CameraController.m_Pitch -= e.RelY * mouseSpeed;

		m_CameraController.m_Pitch = Clamp(m_CameraController.m_Pitch, -90.0f, 90.0f);

		m_CameraController.m_Yaw -= e.RelX * mouseSpeed;
	}
}

void DeferredLightingDemo::OnMouseWheel(MouseWheelEventArgs& e)
{
	{
		auto fov = m_Camera.GetFov();

		fov -= e.WheelDelta;
		fov = Clamp(fov, 12.0f, 90.0f);

		m_Camera.SetFov(fov);

		char buffer[256];
		sprintf_s(buffer, "FoV: %f\n", fov);
		OutputDebugStringA(buffer);
	}
}