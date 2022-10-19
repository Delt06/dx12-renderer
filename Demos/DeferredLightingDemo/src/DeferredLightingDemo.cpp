#include <DeferredLightingDemo.h>

#include <DX12Library/Application.h>
#include <DX12Library/CommandQueue.h>
#include <DX12Library/CommandList.h>
#include <DX12Library/Helpers.h>
#include <Framework/Light.h>
#include <DX12Library/Window.h>
#include <Framework/GameObject.h>
#include <Framework/Bone.h>
#include <Framework/Animation.h>
#include <DX12Library/Cubemap.h>

#include <wrl.h>

#include <Framework/GraphicsSettings.h>
#include <Framework/Model.h>
#include <Framework/ModelLoader.h>

using namespace Microsoft::WRL;

#include <d3d12.h>
#include <d3dx12.h>
#include <DirectXColors.h>
#include <Framework/MatricesCb.h>
#include <Framework/Shader.h>
#include <Framework/Material.h>
#include <Framework/PipelineStateBuilder.h>

using namespace DirectX;

#include <algorithm> // For std::min and std::max.
#if defined(min)
#undef min
#endif

#include <PBR/DiffuseIrradiance.h>
#include <PBR/BrdfIntegration.h>
#include <PBR/PreFilterEnvironment.h>
#include <Framework/Blit.h>
#include <HDR/AutoExposure.h>
#include <TaaCBuffer.h>
#include <Taa.h>
#include <DX12Library/ShaderUtils.h>
#include <DX12Library/ClearValue.h>

#if defined(max)
#undef max
#endif

namespace
{
	constexpr DXGI_FORMAT gBufferFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
	constexpr DXGI_FORMAT lightBufferFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
	constexpr DXGI_FORMAT resultFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	constexpr DXGI_FORMAT depthBufferFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	constexpr UINT gBufferTexturesCount = 5;

	namespace ClearColors
	{
		constexpr FLOAT CLEAR_COLOR[] = { 0.4f, 0.6f, 0.9f, 1.0f };
		constexpr FLOAT VELOCITY_CLEAR_COLOR[] = { 0.0f, 0.0f, 0.0f, 0.0f };
		constexpr FLOAT LIGHT_BUFFER_CLEAR_COLOR[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	}

	static ClearValue gBufferColorClearValue = ClearValue(gBufferFormat, ClearColors::CLEAR_COLOR);
	static ClearValue velocityColorClearValue = ClearValue(gBufferFormat, ClearColors::VELOCITY_CLEAR_COLOR);
	static ClearValue lightBufferColorClearValue = ClearValue(lightBufferFormat, ClearColors::LIGHT_BUFFER_CLEAR_COLOR);
}

namespace Demo::Pipeline
{
	struct CBuffer
	{
		DirectX::XMMATRIX m_View;
		DirectX::XMMATRIX m_Projection;
		DirectX::XMMATRIX m_ViewProjection;

		DirectX::XMFLOAT4 m_CameraPosition;

		DirectX::XMMATRIX m_InverseView;
		DirectX::XMMATRIX m_InverseProjection;

		DirectX::XMFLOAT2 m_ScreenResolution;
		DirectX::XMFLOAT2 m_ScreenTexelSize;

		DirectX::XMFLOAT2 m_Taa_JitterOffset;
		float m_Padding[2];
	};
}

namespace Demo::Model
{
	struct CBuffer
	{
		DirectX::XMMATRIX m_Model;
		DirectX::XMMATRIX m_ModelViewProjection;
		DirectX::XMMATRIX m_InverseTransposeModel;

		DirectX::XMMATRIX m_Taa_PreviousModelViewProjectionMatrix;

		void Compute(const DirectX::XMMATRIX& model, const DirectX::XMMATRIX& viewProjection)
		{
			m_Model = model;
			m_ModelViewProjection = model * viewProjection;
			m_InverseTransposeModel = XMMatrixTranspose(XMMatrixInverse(nullptr, model));
		}
	};
}

namespace
{
	// Clamp a value between a min and max range.
	template<typename T>
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

	// Builds a look-at (world) matrix from a point, up and direction vectors.
	XMMATRIX XM_CALLCONV
		LookAtMatrix(FXMVECTOR
			position,
			FXMVECTOR direction, FXMVECTOR
			up)
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

		return
			M;
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

	CD3DX12_BLEND_DESC AdditiveBlending()
	{
		auto desc = CD3DX12_BLEND_DESC(CD3DX12_DEFAULT());
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
		const auto l = 0.01f / GetMaxColorComponentRGB(light.Color);
		const auto lInv = 1 / l;
		auto discriminant = light.LinearAttenuation * light.LinearAttenuation
			- 4 * light.QuadraticAttenuation * (light.ConstantAttenuation - lInv);
		auto radius = 2 * (-light.LinearAttenuation + sqrt(discriminant)) / (2 * light.QuadraticAttenuation);
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
		auto skyboxSampler = CD3DX12_STATIC_SAMPLER_DESC(shaderRegister,
			D3D12_FILTER_MAXIMUM_MIN_MAG_MIP_LINEAR,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
		skyboxSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		return skyboxSampler;
	}
}

DeferredLightingDemo::DeferredLightingDemo(const std::wstring& name,
	int width,
	int height,
	GraphicsSettings graphicsSettings)
	: Base(name, width, height, graphicsSettings.VSync),
	m_GraphicsSettings(graphicsSettings),
	m_CameraController{},
	m_Width(0),
	m_Height(0)
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

	m_CommonRootSignature = std::make_shared<CommonRootSignature>(m_WhiteTexture2d);

	// root signature and pipeline states for light passes
	{
		m_FullScreenMesh = Mesh::CreateBlitTriangle(*commandList);

		// directional light pass
		{
			auto shader = std::make_shared<Shader>(m_CommonRootSignature,
				ShaderBlob(L"DeferredLightingDemo_LightBuffer_Directional_VS.cso"),
				ShaderBlob(L"DeferredLightingDemo_LightBuffer_Directional_PS.cso"),
				[](PipelineStateBuilder& builder)
				{
					builder
						.WithAdditiveBlend()
						.WithDisabledDepthStencil()
						;
				}
			);
			m_DirectionalLightPassMaterial = Material::Create(shader);
		}

		// light stencil pass
		{
			auto shader = std::make_shared<Shader>(
				m_CommonRootSignature,
				ShaderBlob(L"DeferredLightingDemo_LightBuffer_LightStencil_VS.cso"),
				ShaderBlob(L"DeferredLightingDemo_LightBuffer_LightStencil_PS.cso"),
				[](PipelineStateBuilder& builder)
				{
					// https://ogldev.org/www/tutorial37/tutorial37.html
					// no back-face culling
					auto rasterizer = CD3DX12_RASTERIZER_DESC(D3D12_FILL_MODE_SOLID, D3D12_CULL_MODE_NONE, FALSE, 0, 0,
						0, TRUE, FALSE, FALSE, 0,
						D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF);

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

					builder
						.WithRasterizer(rasterizer)
						.WithAdditiveBlend()
						.WithDepthStencil(depthStencil)
						;
				}
			);
			m_LightStencilPasssMaterial = Material::Create(shader);
		}

		const auto CreateLightPassShader = [this](
			const std::wstring& vertexShaderPath,
			const std::wstring& pixelShaderPath
			)
		{
			return std::make_shared<Shader>(m_CommonRootSignature,
				ShaderBlob(vertexShaderPath),
				ShaderBlob(pixelShaderPath),
				[](PipelineStateBuilder& builder)
				{
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

					builder
						.WithRasterizer(CD3DX12_RASTERIZER_DESC(D3D12_FILL_MODE_SOLID, D3D12_CULL_MODE_FRONT, FALSE, 0, 0,
							0, TRUE, FALSE, FALSE, 0,
							D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF))
						.WithAdditiveBlend()
						.WithDepthStencil(depthStencil)
						;
				}
			);
		};

		// point light pass
		{
			m_PointLightMesh = Mesh::CreateSphere(*commandList, 1.0f);
			m_PointLightPassMaterial = Material::Create(
				CreateLightPassShader(
					L"DeferredLightingDemo_LightBuffer_Common_VS.cso",
					L"DeferredLightingDemo_LightBuffer_Point_PS.cso")
			);
		}

		// spotlight pass
		{
			m_SpotLightMesh = Mesh::CreateSpotlightPyramid(*commandList, 1, 1);
			m_SpotLightPassMaterial = Material::Create(
				CreateLightPassShader(
					L"DeferredLightingDemo_LightBuffer_Common_VS.cso",
					L"DeferredLightingDemo_LightBuffer_Spot_PS.cso"
				)
			);
		}

		// capsule light pass
		{
			ModelLoader modelLoader;
			m_CapsuleLightMesh = modelLoader.Load(*commandList, "Assets/Models/_builtin/cylinder.obj")->GetMeshes()[0];
			m_CapsuleLightPassMaterial = Material::Create(
				CreateLightPassShader(
					L"DeferredLightingDemo_LightBuffer_Common_VS.cso",
					L"DeferredLightingDemo_LightBuffer_Capsule_PS.cso"
				)
			);
		}

		// skybox pass
		{
			auto shader = std::make_shared<Shader>(m_CommonRootSignature,
				ShaderBlob(L"DeferredLightingDemo_Skybox_VS.cso"),
				ShaderBlob(L"DeferredLightingDemo_Skybox_PS.cso"),
				[](PipelineStateBuilder& builder)
				{
					auto rasterizer = CD3DX12_RASTERIZER_DESC(D3D12_FILL_MODE_SOLID, D3D12_CULL_MODE_FRONT, FALSE, 0, 0,
						0, TRUE, FALSE, FALSE, 0,
						D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF);
					auto depthStencil = CD3DX12_DEPTH_STENCIL_DESC(CD3DX12_DEFAULT());
					depthStencil.DepthEnable = true; // read
					depthStencil.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // but not write

					builder.WithRasterizer(rasterizer)
						.WithDepthStencil(depthStencil)
						;

				}
			);
			m_SkyboxPassMaterial = Material::Create(shader);
			m_SkyboxMesh = Mesh::CreateCube(*commandList);
		}

		{
			m_Ssao = std::make_unique<Ssao>(m_CommonRootSignature, *commandList, m_Width, m_Height, true);
		}

		{
			m_Ssr = std::make_unique<Ssr>(m_CommonRootSignature, *commandList, lightBufferFormat, m_Width, m_Height, true);
		}

		{
			m_Bloom = std::make_unique<Bloom>(m_CommonRootSignature, *commandList, m_Width, m_Height, lightBufferFormat, 4);
		}

		{
			m_AutoExposurePso = std::make_unique<AutoExposure>(m_CommonRootSignature, *commandList);
			m_ToneMappingPso = std::make_unique<ToneMapping>(m_CommonRootSignature, *commandList);
		}

		{
			m_Taa = std::make_unique<Taa>(m_CommonRootSignature, *commandList, resultFormat, m_Width, m_Height);
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

		const auto gBufferShader = std::make_shared<Shader>(m_CommonRootSignature,
			ShaderBlob(L"DeferredLightingDemo_GBuffer_VS.cso"),
			ShaderBlob(L"DeferredLightingDemo_GBuffer_PS.cso")
			);

		const std::string property_Metallic = "Metallic";
		const std::string property_Roughness = "Roughness";
		const std::string property_Diffuse = "Diffuse";
		const std::string property_Emission = "Emission";
		const std::string property_TilingOffset = "TilingOffset";

		const std::string property_diffuseMap = "diffuseMap";
		const std::string property_normalMap = "normalMap";
		const std::string property_metallicMap = "metallicMap";
		const std::string property_roughnessMap = "roughnessMap";
		const std::string property_ambientOcclusionMap = "ambientOcclusionMap";

		const auto MaterialSetTexture = [&modelLoader, &commandList](Material& material, const std::string& propertyName, const std::wstring& texturePath, TextureUsageType usage = TextureUsageType::Albedo)
		{
			material.SetShaderResourceView(propertyName, ShaderResourceView(modelLoader.LoadTexture(*commandList, texturePath, usage)));
		};

		{
			auto model = modelLoader.LoadExisting(Mesh::CreatePlane(*commandList));
			auto material = Material::Create(gBufferShader);

			material->SetVariable(property_Metallic, 1.0f);
			material->SetVariable(property_Roughness, 0.5f);
			material->SetVariable(property_TilingOffset, XMFLOAT4(6, 6, 0, 0));

			MaterialSetTexture(*material, property_diffuseMap, L"Assets/Textures/Ground047/Ground047_1K_Color.jpg");
			MaterialSetTexture(*material, property_normalMap, L"Assets/Textures/Ground047/Ground047_1K_NormalDX.jpg", TextureUsageType::Normalmap);
			MaterialSetTexture(*material, property_roughnessMap, L"Assets/Textures/Ground047/Ground047_1K_Roughness.jpg", TextureUsageType::Other);
			MaterialSetTexture(*material, property_ambientOcclusionMap, L"Assets/Textures/Ground047/Ground047_1K_AmbientOcclusion.jpg", TextureUsageType::Other);

			XMMATRIX translationMatrix = XMMatrixTranslation(0.0f, 0.0f, 0.0f);
			XMMATRIX rotationMatrix = XMMatrixIdentity();
			XMMATRIX scaleMatrix = XMMatrixScaling(200.0f, 200.0f, 200.0f);
			XMMATRIX worldMatrix = scaleMatrix * translationMatrix * rotationMatrix;
			m_GameObjects.push_back(GameObject(worldMatrix, model, material));
		}

		{
			auto model = modelLoader.Load(*commandList, "Assets/Models/old-wooden-chest/chest_01.fbx");
			auto material = Material::Create(gBufferShader);

			MaterialSetTexture(*material, property_diffuseMap, L"Assets/Models/old-wooden-chest/chest_01_BaseColor.png");
			MaterialSetTexture(*material, property_normalMap, L"Assets/Models/old-wooden-chest/chest_01_Normal.png", TextureUsageType::Normalmap);
			MaterialSetTexture(*material, property_roughnessMap, L"Assets/Models/old-wooden-chest/chest_01_Roughness.png", TextureUsageType::Other);
			MaterialSetTexture(*material, property_metallicMap, L"Assets/Models/old-wooden-chest/chest_01_Metallic.png", TextureUsageType::Other);

			{
				XMMATRIX translationMatrix = XMMatrixTranslation(0.0f, 0.25f, 15.0f);
				XMMATRIX rotationMatrix = XMMatrixRotationRollPitchYaw(XMConvertToRadians(90.0f), 0.0f, 0.0f);
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
			auto material = Material::Create(gBufferShader);
			material->SetVariable(property_Metallic, 1.0f);
			material->SetVariable(property_Roughness, 0.0f);

			XMMATRIX translationMatrix = XMMatrixTranslation(-50.0f, 0.1f, 15.0f);
			XMMATRIX rotationMatrix = XMMatrixIdentity();
			XMMATRIX scaleMatrix = XMMatrixScaling(30.0f, 30.0f, 30.0f);
			XMMATRIX worldMatrix = scaleMatrix * translationMatrix * rotationMatrix;
			m_GameObjects.emplace_back(worldMatrix, model, material);
		}

		{
			auto model = modelLoader.LoadExisting(Mesh::CreateCube(*commandList));
			auto material = Material::Create(gBufferShader);
			material->SetVariable(property_Metallic, 0.01f);
			material->SetVariable(property_Roughness, 0.25f);

			XMMATRIX translationMatrix = XMMatrixTranslation(-54.0f, 2.5f, 7.0f);
			XMMATRIX rotationMatrix = XMMatrixRotationRollPitchYaw(0, 0, 0);
			XMMATRIX scaleMatrix = XMMatrixScaling(5.0f, 5.0f, 5.0f);
			XMMATRIX worldMatrix = scaleMatrix * rotationMatrix * translationMatrix;
			m_GameObjects.emplace_back(worldMatrix, model, material);
		}

		{
			auto model = modelLoader.Load(*commandList, "Assets/Models/cerberus/Cerberus_LP.FBX");
			auto material = Material::Create(gBufferShader);

			MaterialSetTexture(*material, property_diffuseMap, L"Assets/Models/cerberus/Cerberus_A.jpg");
			MaterialSetTexture(*material, property_normalMap, L"Assets/Models/cerberus/Cerberus_N.jpg", TextureUsageType::Normalmap);
			MaterialSetTexture(*material, property_roughnessMap, L"Assets/Models/cerberus/Cerberus_R.jpg", TextureUsageType::Other);
			MaterialSetTexture(*material, property_metallicMap, L"Assets/Models/cerberus/Cerberus_M.jpg", TextureUsageType::Other);

			XMMATRIX translationMatrix = XMMatrixTranslation(15.0f, 5.0f, 10.0f);
			XMMATRIX rotationMatrix =
				XMMatrixRotationRollPitchYaw(XMConvertToRadians(90), XMConvertToRadians(135), XMConvertToRadians(0));
			XMMATRIX scaleMatrix = XMMatrixScaling(0.10f, 0.10f, 0.10f);
			XMMATRIX worldMatrix = scaleMatrix * rotationMatrix * translationMatrix;
			m_GameObjects.emplace_back(worldMatrix, model, material);
		}

		{
			auto model = modelLoader.Load(*commandList, "Assets/Models/tv/TV.FBX");
			auto material = Material::Create(gBufferShader);
			material->SetVariable(property_Metallic, 1.0f);
			material->SetVariable(property_Roughness, 1.0f);

			MaterialSetTexture(*material, property_diffuseMap, L"Assets/Models/tv/TV_Color.jpg");
			MaterialSetTexture(*material, property_normalMap, L"Assets/Models/tv/TV_Normal.jpg", TextureUsageType::Normalmap);
			MaterialSetTexture(*material, property_roughnessMap, L"Assets/Models/tv/TV_Roughness.jpg", TextureUsageType::Other);
			MaterialSetTexture(*material, property_metallicMap, L"Assets/Models/tv/TV_Metallic.jpg", TextureUsageType::Other);
			MaterialSetTexture(*material, property_ambientOcclusionMap, L"Assets/Models/tv/TV_Occlusion.jpg", TextureUsageType::Other);

			XMMATRIX translationMatrix = XMMatrixTranslation(-14.0f, 0.0f, 18.0f);
			XMMATRIX rotationMatrix =
				XMMatrixRotationRollPitchYaw(XMConvertToRadians(90), XMConvertToRadians(-45), XMConvertToRadians(0));
			XMMATRIX scaleMatrix = XMMatrixScaling(0.30f, 0.30f, 0.30f);
			XMMATRIX worldMatrix = scaleMatrix * rotationMatrix * translationMatrix;
			m_GameObjects.emplace_back(worldMatrix, model, material);
		}

		{
			const int steps = 5;
			for (auto x = 0; x < steps; ++x)
			{
				for (auto y = 0; y < steps; ++y)
				{
					auto model = modelLoader.LoadExisting(Mesh::CreateSphere(*commandList));
					auto material = Material::Create(gBufferShader);
					material->SetVariable(property_Metallic, static_cast<float>(x) / (steps - 1));
					material->SetVariable(property_Roughness, static_cast<float>(y) / (steps - 1));

					XMMATRIX translationMatrix = XMMatrixTranslation(x * 1.5f, 5.0f + y * 2.0f, 25.0f);
					XMMATRIX worldMatrix = translationMatrix;
					m_GameObjects.emplace_back(worldMatrix, model, material);
				}
			}
		}

		{
			for (const auto& pointLight : m_PointLights)
			{
				auto mesh = modelLoader.LoadExisting(Mesh::CreateSphere(*commandList, 0.5f));
				auto material = Material::Create(gBufferShader);

				material->SetVariable(property_Metallic, 0.2f);
				material->SetVariable(property_Roughness, 0.25f);
				material->SetVariable(property_Diffuse, NormalizeColor(pointLight.Color));
				material->SetVariable(property_Emission, GetColorMagnitude(pointLight.Color));

				auto positionWS = XMLoadFloat4(&pointLight.PositionWs);
				auto modelMatrix = XMMatrixTranslationFromVector(positionWS);
				m_GameObjects.emplace_back(modelMatrix, mesh, material);
			}
		}

		m_Skybox = std::make_shared<Texture>();
		commandList->LoadTextureFromFile(*m_Skybox, L"Assets/Textures/skybox/skybox.dds", TextureUsageType::Albedo);
	}

	m_CommonRootSignature->Bind(*commandList);

	{
		PIXScope(*commandList, "Diffuse Irradiance Convolution");

		const uint32_t arraySize = Cubemap::SIDES_COUNT;
		D3D12_RESOURCE_DESC skyboxDesc = m_Skybox->GetD3D12ResourceDesc();

		const UINT diffuseIrradianceMapSize = 128;
		const auto diffuseIrradianceMapDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			DXGI_FORMAT_R16G16B16A16_FLOAT,
			diffuseIrradianceMapSize, diffuseIrradianceMapSize,
			arraySize, 1,
			1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

		auto diffuseIrradianceMap = std::make_shared<Texture>(diffuseIrradianceMapDesc, nullptr,
			TextureUsageType::Albedo,
			L"Skybox IBL (Diffuse Irradiance)");
		m_DiffuseIrradianceMapRt.AttachTexture(Color0, diffuseIrradianceMap);

		DiffuseIrradiance diffuseIrradiancePso(m_CommonRootSignature, *commandList);
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
		D3D12_RESOURCE_DESC skyboxDesc = m_Skybox->GetD3D12ResourceDesc();

		const UINT brdfIntegrationMapSize = 512;
		const auto diffuseIrradianceMapDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			DXGI_FORMAT_R16G16_FLOAT,
			brdfIntegrationMapSize, brdfIntegrationMapSize,
			arraySize, 1,
			1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

		auto brdfIntegrationMap = std::make_shared<Texture>(diffuseIrradianceMapDesc, nullptr,
			TextureUsageType::Other,
			L"BRDF Integration Map");
		m_BrdfIntegrationMapRt.AttachTexture(Color0, brdfIntegrationMap);

		BrdfIntegration brdfIntegrationPso(m_CommonRootSignature, *commandList);
		brdfIntegrationPso.SetRenderTarget(*commandList, m_BrdfIntegrationMapRt);
		brdfIntegrationPso.Draw(*commandList);
	}

	{
		PIXScope(*commandList, "Pre-Filter Environment");

		const uint32_t arraySize = Cubemap::SIDES_COUNT;
		D3D12_RESOURCE_DESC skyboxDesc = m_Skybox->GetD3D12ResourceDesc();

		const UINT mipLevels = 5;

		const auto preFilterEnvironmentMapDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			DXGI_FORMAT_R16G16B16A16_FLOAT,
			skyboxDesc.Width, skyboxDesc.Height,
			arraySize, mipLevels,
			1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

		auto preFilterEnvironmentMap = std::make_shared<Texture>(preFilterEnvironmentMapDesc, nullptr,
			TextureUsageType::Albedo,
			L"Skybox IBL (Pre-Filtered Environment Map)");
		m_PreFilterEnvironmentMapRt.AttachTexture(Color0, preFilterEnvironmentMap);

		PreFilterEnvironment preFilterEnvironmentPso(m_CommonRootSignature, *commandList);
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

	{
		m_ReflectionsPass = std::make_unique<Reflections>(m_CommonRootSignature, *commandList);
	}

	std::shared_ptr<Texture> depthBuffer;

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

		depthBuffer = std::make_shared<Texture>(depthDesc, &depthClearValue,
			TextureUsageType::Depth,
			L"Depth-Stencil");
		m_DepthTexture = std::make_shared<Texture>(depthDesc, &depthClearValue,
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

		auto diffuseTexture = std::make_shared<Texture>(colorDesc, gBufferColorClearValue,
			TextureUsageType::RenderTarget,
			L"GBuffer-Diffuse");

		auto normalTexture = std::make_shared<Texture>(colorDesc, gBufferColorClearValue,
			TextureUsageType::RenderTarget,
			L"GBuffer-Normal");

		auto surfaceTexture = std::make_shared<Texture>(colorDesc, gBufferColorClearValue,
			TextureUsageType::RenderTarget,
			L"GBuffer-Surface");

		auto velocityTexture = std::make_shared<Texture>(colorDesc, velocityColorClearValue,
			TextureUsageType::RenderTarget,
			L"GBuffer-Velocity");

		m_GBufferRenderTarget.AttachTexture(GetGBufferTextureAttachmentPoint(GBufferTextureType::Diffuse),
			diffuseTexture);
		m_GBufferRenderTarget.AttachTexture(GetGBufferTextureAttachmentPoint(GBufferTextureType::Normals),
			normalTexture);
		m_GBufferRenderTarget.AttachTexture(GetGBufferTextureAttachmentPoint(GBufferTextureType::Surface),
			surfaceTexture);
		m_GBufferRenderTarget.AttachTexture(GetGBufferTextureAttachmentPoint(GBufferTextureType::Velocity),
			velocityTexture);
		m_GBufferRenderTarget.AttachTexture(GetGBufferTextureAttachmentPoint(GBufferTextureType::DepthStencil),
			depthBuffer);

		m_SurfaceRenderTarget.AttachTexture(Color0, GetGBufferTexture(GBufferTextureType::Surface));
	}

	// Light Buffer
	{
		auto colorDesc = CD3DX12_RESOURCE_DESC::Tex2D(lightBufferFormat,
			m_Width, m_Height,
			1, 1,
			1, 0,
			D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

		auto lightBuffer = std::make_shared<Texture>(colorDesc, lightBufferColorClearValue,
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

		auto resultRT = std::make_shared<Texture>(colorDesc, nullptr,
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

		m_GBufferRenderTarget.Resize(m_Width, m_Height);
		m_LightBufferRenderTarget.Resize(m_Width, m_Height);
		m_LightBufferRenderTarget.AttachTexture(DepthStencil, m_GBufferRenderTarget.GetTexture(DepthStencil));
		m_LightStencilRenderTarget.AttachTexture(DepthStencil, m_GBufferRenderTarget.GetTexture(DepthStencil));

		if (m_DepthTexture != nullptr)
		{
			m_DepthTexture->Resize(m_Width, m_Height);
		}

		m_ResultRenderTarget.Resize(m_Width, m_Height);

		if (m_Ssao != nullptr)
		{
			m_Ssao->Resize(m_Width, m_Height);
		}

		if (m_Bloom != nullptr)
		{
			m_Bloom->Resize(m_Width, m_Height);
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
		double fps = static_cast<double>(frameCount) / totalTime;

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
		PIXScope(*commandList, "OnRender");


		{
			PIXScope(*commandList, "Clear Render Targets");

			{
				PIXScope(*commandList, "Clear GBuffer");
				commandList->ClearRenderTarget(m_GBufferRenderTarget,
					gBufferColorClearValue,
					D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL);
				commandList->ClearTexture(*GetGBufferTexture(GBufferTextureType::Velocity), velocityColorClearValue);
			}

			{
				PIXScope(*commandList, "Clear Light Buffer");
				commandList->ClearTexture(*m_LightBufferRenderTarget.GetTexture(Color0), lightBufferColorClearValue);
			}
		}

		const XMMATRIX viewMatrix = m_Camera.GetViewMatrix();
		const XMMATRIX projectionMatrix = m_Camera.GetProjectionMatrix();
		const XMMATRIX viewProjectionMatrix = viewMatrix * projectionMatrix;

		m_CommonRootSignature->Bind(*commandList);
		Demo::Pipeline::CBuffer pipelineCBuffer{};

		{
			pipelineCBuffer.m_View = viewMatrix;
			pipelineCBuffer.m_Projection = projectionMatrix;
			pipelineCBuffer.m_ViewProjection = viewProjectionMatrix;

			XMStoreFloat4(&pipelineCBuffer.m_CameraPosition, m_Camera.GetTranslation());

			pipelineCBuffer.m_InverseView = XMMatrixInverse(nullptr, viewMatrix);
			pipelineCBuffer.m_InverseProjection = XMMatrixInverse(nullptr, projectionMatrix);

			pipelineCBuffer.m_ScreenResolution.x = static_cast<float>(m_Width);
			pipelineCBuffer.m_ScreenResolution.y = static_cast<float>(m_Height);
			pipelineCBuffer.m_ScreenTexelSize.x = 1.0f / static_cast<float>(m_Width);
			pipelineCBuffer.m_ScreenTexelSize.y = 1.0f / static_cast<float>(m_Height);

			pipelineCBuffer.m_Taa_JitterOffset = m_TaaEnabled ? m_Taa->ComputeJitterOffset(m_Width, m_Height) : XMFLOAT2(0.0f, 0.0f);
			m_CommonRootSignature->SetPipelineConstantBuffer(*commandList, pipelineCBuffer);
		}


		{
			PIXScope(*commandList, "GBuffer Pass");

			commandList->SetRenderTarget(m_GBufferRenderTarget);
			commandList->SetAutomaticViewportAndScissorRect(m_GBufferRenderTarget);

			for (const auto& go : m_GameObjects)
			{
				Demo::Model::CBuffer modelCBuffer{};
				modelCBuffer.Compute(go.GetWorldMatrix(), viewProjectionMatrix);
				modelCBuffer.m_Taa_PreviousModelViewProjectionMatrix = go.GetPreviousWorldMatrix() * m_Taa->GetPreviousViewProjectionMatrix();

				m_CommonRootSignature->SetModelConstantBuffer(*commandList, modelCBuffer);
				go.Draw(*commandList);
			}
		}

		{
			PIXScope(*commandList, "Copy Depth Buffer to Depth Texture");

			commandList->CopyResource(*m_DepthTexture, *m_GBufferRenderTarget.GetTexture(DepthStencil));
		}

		auto BindGBuffer = [this, &commandList]()
		{
			m_CommonRootSignature->SetPipelineShaderResourceView(*commandList,
				0, ShaderResourceView(GetGBufferTexture(GBufferTextureType::Diffuse))
			);

			m_CommonRootSignature->SetPipelineShaderResourceView(*commandList,
				1, ShaderResourceView(GetGBufferTexture(GBufferTextureType::Normals))
			);

			m_CommonRootSignature->SetPipelineShaderResourceView(*commandList,
				2, ShaderResourceView(GetGBufferTexture(GBufferTextureType::Surface))
			);

			auto depthStencilSrvDesc = GetDepthTextureSrv();
			m_CommonRootSignature->SetPipelineShaderResourceView(*commandList,
				3, ShaderResourceView(m_DepthTexture, 0, 1, &depthStencilSrvDesc)
			);
		};

		BindGBuffer();

		if (m_SsaoEnabled)
		{
			PIXScope(*commandList, "SSAO");

			D3D12_SHADER_RESOURCE_VIEW_DESC depthTextureSrv = GetDepthTextureSrv();
			constexpr float radius = 0.5f;
			constexpr float power = 4.0f;
			m_Ssao->SsaoPass(*commandList,
				*GetGBufferTexture(GBufferTextureType::Normals),
				*m_DepthTexture,
				&depthTextureSrv,
				radius,
				power);

			// make sure surface is not bound as an SRV
			m_CommonRootSignature->SetPipelineShaderResourceView(*commandList,
				2, ShaderResourceView(m_WhiteTexture2d)
			);
			m_Ssao->BlurPass(*commandList, m_SurfaceRenderTarget);
		}

		BindGBuffer();

		if (m_SsrEnabled)
		{
			m_Ssr->Execute(*commandList);
		}

		{
			PIXScope(*commandList, "Light Passes");

			commandList->SetRenderTarget(m_LightBufferRenderTarget);
			commandList->SetAutomaticViewportAndScissorRect(m_LightBufferRenderTarget);

			{
				PIXScope(*commandList, "Directional Light Pass");

				const auto& diffuseIrradianceMap = m_DiffuseIrradianceMapRt.GetTexture(Color0);
				D3D12_SHADER_RESOURCE_VIEW_DESC irradianceSrvDesc;
				irradianceSrvDesc.Format = diffuseIrradianceMap->GetD3D12ResourceDesc().Format;
				irradianceSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				irradianceSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
				irradianceSrvDesc.TextureCube.MipLevels = 1;
				irradianceSrvDesc.TextureCube.MostDetailedMip = 0;
				irradianceSrvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
				ShaderResourceView irradianceMapSrv(diffuseIrradianceMap);
				irradianceMapSrv.m_Desc = &irradianceSrvDesc;

				m_DirectionalLightPassMaterial->SetShaderResourceView("irradianceMap", irradianceMapSrv);
				m_DirectionalLightPassMaterial->SetVariable<XMFLOAT4>("DirectionWS", m_DirectionalLight.m_DirectionWs);
				m_DirectionalLightPassMaterial->SetVariable<XMFLOAT4>("Color", m_DirectionalLight.m_Color);

				m_DirectionalLightPassMaterial->Bind(*commandList);
				m_FullScreenMesh->Draw(*commandList);
			}

			{
				D3D12_SHADER_RESOURCE_VIEW_DESC preFilterEnvironmentSrvDesc;
				preFilterEnvironmentSrvDesc.Format =
					m_PreFilterEnvironmentMapRt.GetTexture(Color0)->GetD3D12ResourceDesc().Format;
				preFilterEnvironmentSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				preFilterEnvironmentSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
				preFilterEnvironmentSrvDesc.TextureCube.MipLevels = -1;
				preFilterEnvironmentSrvDesc.TextureCube.MostDetailedMip = 0;
				preFilterEnvironmentSrvDesc.TextureCube.ResourceMinLODClamp = 0.0f;

				m_ReflectionsPass->SetPreFilterEnvironmentSrvDesc(preFilterEnvironmentSrvDesc);

				const auto& preFilterMap = m_PreFilterEnvironmentMapRt.GetTexture(Color0);
				const auto& brdfLut = m_BrdfIntegrationMapRt.GetTexture(Color0);
				const auto& ssrTexture = m_SsrEnabled ? m_Ssr->GetReflectionsTexture() : m_Ssr->GetEmptyReflectionsTexture();

				m_ReflectionsPass->Draw(*commandList,
					preFilterMap,
					brdfLut,
					ssrTexture
				);
			}

			commandList->SetStencilRef(0);

			{
				PIXScope(*commandList, "Point Light Pass");

				for (const auto& pointLight : m_PointLights)
				{
					XMMATRIX modelMatrix = GetModelMatrix(pointLight);
					const auto& mesh = m_PointLightMesh;
					LightStencilPass(*commandList, modelMatrix, viewProjectionMatrix, mesh);
					PointLightPass(*commandList, pointLight, mesh);
				}
			}

			{
				PIXScope(*commandList, "Spot Light Pass");

				for (const auto& spotLight : m_SpotLights)
				{
					XMMATRIX modelMatrix = GetModelMatrix(spotLight);
					const auto& mesh = m_SpotLightMesh;
					LightStencilPass(*commandList, modelMatrix, viewProjectionMatrix, mesh);
					SpotLightPass(*commandList, spotLight, mesh);
				}
			}

			{
				PIXScope(*commandList, "Capsule Light Pass");

				for (const auto& capsuleLight : m_CapsuleLights)
				{
					XMMATRIX modelMatrix = GetModelMatrix(capsuleLight);
					const auto& mesh = m_CapsuleLightMesh;
					LightStencilPass(*commandList, modelMatrix, viewProjectionMatrix, mesh);
					CapsuleLightPass(*commandList, capsuleLight, mesh);
				}
			}
		}

		{
			PIXScope(*commandList, "Skybox Pass");

			commandList->SetRenderTarget(m_LightBufferRenderTarget);
			commandList->SetAutomaticViewportAndScissorRect(m_LightBufferRenderTarget);

			const XMMATRIX
				modelMatrix = XMMatrixScaling(2, 2, 2) * XMMatrixTranslationFromVector(m_Camera.GetTranslation());
			Demo::Model::CBuffer modelCBuffer{};
			modelCBuffer.Compute(modelMatrix, viewProjectionMatrix);
			m_CommonRootSignature->SetModelConstantBuffer(*commandList, modelCBuffer);

			const auto& skybox = m_Skybox;

			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
			srvDesc.Format = skybox->GetD3D12ResourceDesc().Format;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
			srvDesc.TextureCube.MipLevels = 1;
			srvDesc.TextureCube.MostDetailedMip = 0;
			srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;

			auto skyboxSrv = ShaderResourceView(skybox);
			skyboxSrv.m_Desc = &srvDesc;
			m_SkyboxPassMaterial->SetShaderResourceView("skybox", skyboxSrv);

			m_SkyboxPassMaterial->Bind(*commandList);
			m_SkyboxMesh->Draw(*commandList);
			m_SkyboxPassMaterial->Unbind(*commandList);
		}

		if (m_SsrEnabled)
		{
			m_Ssr->CaptureSceneColor(*commandList, *m_LightBufferRenderTarget.GetTexture(Color0));
		}

		if (m_BloomEnabled)
		{
			BloomParameters parameters{};
			parameters.Threshold = 1.0f;
			parameters.SoftThreshold = 0.1f;
			parameters.Intensity = 0.35f;
			m_Bloom->Draw(*commandList, m_LightBufferRenderTarget.GetTexture(Color0), m_LightBufferRenderTarget, parameters);
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
			const auto& currentBuffer = m_ResultRenderTarget.GetTexture(Color0);
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
	}

	commandQueue->ExecuteCommandList(commandList);
	const auto& presentedTexture = m_TaaEnabled ? m_Taa->GetResolvedTexture() : m_ResultRenderTarget.GetTexture(Color0);
	PWindow->Present(*presentedTexture);
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

const std::shared_ptr<Texture>& DeferredLightingDemo::GetGBufferTexture(GBufferTextureType type)
{
	return m_GBufferRenderTarget.GetTexture(GetGBufferTextureAttachmentPoint(type));
}

void DeferredLightingDemo::LightStencilPass(CommandList& commandList,
	const DirectX::XMMATRIX& lightWorldMatrix,
	const DirectX::XMMATRIX& viewProjectionMatrix,
	const std::shared_ptr<Mesh>& mesh)
{
	PIXScope(commandList, "Light Stencil Pass");

	commandList.SetRenderTarget(m_LightStencilRenderTarget);
	commandList.ClearDepthStencilTexture(*m_LightStencilRenderTarget.GetTexture(DepthStencil), D3D12_CLEAR_FLAG_STENCIL);

	Demo::Model::CBuffer modelCBuffer{};
	modelCBuffer.Compute(lightWorldMatrix, viewProjectionMatrix);
	m_CommonRootSignature->SetModelConstantBuffer(commandList, modelCBuffer);

	m_LightStencilPasssMaterial->Bind(commandList);
	mesh->Draw(commandList);
	m_LightStencilPasssMaterial->Unbind(commandList);
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

void DeferredLightingDemo::PointLightPass(CommandList& commandList,
	const PointLight& pointLight,
	const std::shared_ptr<Mesh>& mesh)
{
	m_PointLightPassMaterial->SetAllVariables(pointLight);

	commandList.SetRenderTarget(m_LightBufferRenderTarget);

	m_PointLightPassMaterial->Bind(commandList);
	mesh->Draw(commandList);
	m_PointLightPassMaterial->Unbind(commandList);
}

void DeferredLightingDemo::SpotLightPass(CommandList& commandList,
	const SpotLight& spotLight,
	const std::shared_ptr<Mesh>& mesh)
{
	m_SpotLightPassMaterial->SetAllVariables(spotLight);

	commandList.SetRenderTarget(m_LightBufferRenderTarget);

	m_SpotLightPassMaterial->Bind(commandList);
	mesh->Draw(commandList);
	m_SpotLightPassMaterial->Unbind(commandList);
}

void DeferredLightingDemo::CapsuleLightPass(CommandList& commandList,
	const CapsuleLight& capsuleLight,
	const std::shared_ptr<Mesh>& mesh)
{
	m_CapsuleLightPassMaterial->SetAllVariables(capsuleLight);

	commandList.SetRenderTarget(m_LightBufferRenderTarget);

	m_CapsuleLightPassMaterial->Bind(commandList);
	mesh->Draw(commandList);
	m_CapsuleLightPassMaterial->Unbind(commandList);
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
	case KeyCode::P:
		m_SsrEnabled = !m_SsrEnabled;
		OutputDebugStringA(m_SsrEnabled ? "SSR: On\n" : "SSR: Off\n");
		break;
	case KeyCode::B:
		m_BloomEnabled = !m_BloomEnabled;
		OutputDebugStringA(m_BloomEnabled ? "Bloom: On\n" : "Bloom: Off\n");
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