#include "LightingDemo.h"

#include <Application.h>
#include <CommandQueue.h>
#include <CommandList.h>
#include <Helpers.h>
#include <Light.h>
#include <Material.h>
#include <Window.h>
#include <GameObject.h>

#include <wrl.h>

#include "GraphicsSettings.h"
#include "Model.h"
#include "ModelLoader.h"
#include "ParticleSystem.h"
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

#if defined(max)
#undef max
#endif

namespace
{
	struct LightPropertiesCb
	{
		uint32_t NumPointLights;
	};

	struct DirectionalLightCb
	{
		XMFLOAT4 DirectionVs;
		XMFLOAT4 Color;
	};

	struct ShadowMatricesCb
	{
		XMMATRIX ViewProjection;
	};

	namespace RootParameters
	{
		// An enum for root signature parameters.
		// I'm not using scoped enums to avoid the explicit cast that would be required
		// to use these as root indices in the root signature.
		enum RootParameters
		{
			// ConstantBuffer register(b0);
			MatricesCb,
			// ConstantBuffer register(b0, space1);
			MaterialCb,
			// ConstantBuffer register(b1, space1);
			DirLightCb,
			// ConstantBuffer register(b2);
			LightPropertiesCb,
			// ConstantBuffer register(b1);
			ShadowMatricesCb,
			// Texture2D register(t5);
			ShadowMaps,
			// Texture2D register(t0-t3);
			Textures,
			// StructuredBuffer PointLights : register( t4 );
			PointLights,
			NumRootParameters
		};
	}


	// Clamp a value between a min and max range.
	template <typename T>
	constexpr const T& Clamp(const T& val, const T& min, const T& max)
	{
		return val < min ? min : val > max ? max : val;
	}

	bool allowFullscreenToggle = true;
	constexpr FLOAT CLEAR_COLOR[] = {0.4f, 0.6f, 0.9f, 1.0f};

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
}


LightingDemo::LightingDemo(const std::wstring& name, int width, int height, GraphicsSettings graphicsSettings)
	: Base(name, width, height, graphicsSettings.m_VSync)
	  , m_Viewport(CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)))
	  , m_ScissorRect(CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX))
	  , m_CameraController{}
	  , m_Width(0)
	  , m_Height(0)
	  , m_GraphicsSettings(graphicsSettings)
{
	XMVECTOR cameraPos = XMVectorSet(0, 5, -20, 1);
	XMVECTOR cameraTarget = XMVectorSet(0, 5, 0, 1);
	XMVECTOR cameraUp = XMVectorSet(0, 1, 0, 0);

	m_Camera.SetLookAt(cameraPos, cameraTarget, cameraUp);

	m_PAlignedCameraData = static_cast<CameraData*>(_aligned_malloc(sizeof(CameraData), 16));

	m_PAlignedCameraData->m_InitialPosition = m_Camera.GetTranslation();
	m_PAlignedCameraData->m_InitialQRotation = m_Camera.GetRotation();
}

LightingDemo::~LightingDemo()
{
	_aligned_free(m_PAlignedCameraData);
}

bool LightingDemo::LoadContent()
{
	const auto device = Application::Get().GetDevice();
	const auto commandQueue = Application::Get().GetCommandQueue();
	const auto commandList = commandQueue->GetCommandList();

	// Generate default texture
	{
		m_WhiteTexture2d = std::make_shared<Texture>();
		commandList->LoadTextureFromFile(*m_WhiteTexture2d, L"Assets/Textures/white.png");
	}

	// Load models
	{
		ModelLoader modelLoader(m_WhiteTexture2d);

		{
			auto model = modelLoader.LoadObj(*commandList, L"Assets/Models/teapot/teapot.obj", true);
			{
				modelLoader.LoadMap(*model, *commandList, ModelMaps::Diffuse,
				                    L"Assets/Textures/PavingStones/PavingStones_1K_Color.jpg");
				modelLoader.LoadMap(*model, *commandList, ModelMaps::Normal,
				                    L"Assets/Textures/PavingStones/PavingStones_1K_Normal.jpg");
				modelLoader.LoadMap(*model, *commandList, ModelMaps::Gloss,
				                    L"Assets/Textures/PavingStones/PavingStones_1K_Roughness.jpg");
			}
			model->GetMaterial().SpecularPower = 50.0f;
			XMMATRIX translationMatrix = XMMatrixTranslation(0.0f, 0.0f, 0.0f);
			XMMATRIX rotationMatrix = XMMatrixIdentity();
			XMMATRIX scaleMatrix = XMMatrixScaling(0.1f, 0.1f, 0.1f);
			XMMATRIX worldMatrix = scaleMatrix * rotationMatrix * translationMatrix;
			m_GameObjects.push_back(GameObject(worldMatrix, model));
		}

		{
			auto model = modelLoader.LoadObj(*commandList, L"Assets/Models/sphere/sphere-cylcoords-1k.obj", true);
			{
				model->GetMaterial().SpecularPower = 100.0f;
				modelLoader.LoadMap(*model, *commandList, ModelMaps::Diffuse,
				                    L"Assets/Textures/Metal/Metal_1K_Color.jpg");
				modelLoader.LoadMap(*model, *commandList, ModelMaps::Normal,
				                    L"Assets/Textures/Metal/Metal_1K_Normal.jpg");
				modelLoader.LoadMap(*model, *commandList, ModelMaps::Specular,
				                    L"Assets/Textures/Metal/Metal_1K_Specular.jpg");
				modelLoader.LoadMap(*model, *commandList, ModelMaps::Gloss,
				                    L"Assets/Textures/Metal/Metal_1K_Roughness.jpg");
			}

			XMMATRIX translationMatrix = XMMatrixTranslation(0.0f, 0.0f, 25.0f);
			XMMATRIX rotationMatrix = XMMatrixIdentity();
			XMMATRIX scaleMatrix = XMMatrixScaling(0.1f, 0.1f, 0.1f);
			XMMATRIX worldMatrix = scaleMatrix * rotationMatrix * translationMatrix;
			m_GameObjects.push_back(GameObject(worldMatrix, model));
		}

		{
			auto model = modelLoader.LoadExisting(Mesh::CreatePlane(*commandList));
			{
				model->GetMaterial().SpecularPower = 10.0f;
				modelLoader.LoadMap(*model, *commandList, ModelMaps::Diffuse,
				                    L"Assets/Textures/Moss/Moss_1K_Color.jpg");
				modelLoader.LoadMap(*model, *commandList, ModelMaps::Normal,
				                    L"Assets/Textures/Moss/Moss_1K_Normal.jpg");
				modelLoader.LoadMap(*model, *commandList, ModelMaps::Gloss,
				                    L"Assets/Textures/Moss/Moss_1K_Roughness.jpg");
			}
			XMMATRIX translationMatrix = XMMatrixTranslation(0.0f, 0.0f, 0.0f);
			XMMATRIX rotationMatrix = XMMatrixIdentity();
			XMMATRIX scaleMatrix = XMMatrixScaling(80.0f, 1.0f, 80.0f);
			XMMATRIX worldMatrix = scaleMatrix * translationMatrix * rotationMatrix;
			m_GameObjects.push_back(GameObject(worldMatrix, model));
		}
	}

	// Create lights
	{
		m_PointLightPso = std::make_unique<PointLightPso>(device, *commandList);

		m_DirectionalLight.m_DirectionWs = XMFLOAT4(1.0f, 1.0f, 0.0f, 0.0f);
		m_DirectionalLight.m_Color = XMFLOAT4(0.9f, 0.9f, 0.7f, 0.0f);

		// magenta
		{
			PointLight pointLight(XMFLOAT4(-8, 2, -2, 1));
			pointLight.m_Color = XMFLOAT4(1.0f, 0.0f, 1.0f, 1.0f);
			m_PointLights.push_back(pointLight);
		}

		// yellow-ish
		{
			PointLight pointLight(XMFLOAT4(0, 2, -6, 1));
			pointLight.m_Color = XMFLOAT4(3.0f, 2.0f, 0.25f, 1.0f);
			m_PointLights.push_back(pointLight);
		}

		// cyan-ish
		{
			PointLight pointLight(XMFLOAT4(6, 2, 10, 1));
			pointLight.m_Color = XMFLOAT4(0.0f, 4.0f, 1.5f, 1.0f);
			pointLight.m_LinearAttenuation = 0.14f;
			pointLight.m_QuadraticAttenuation = 0.07f;
			m_PointLights.push_back(pointLight);
		}
	}

	// Setup shadows
	{
		m_DirectionalLightShadowPassPso = std::make_unique<DirectionalLightShadowPassPso>(device, *commandList, m_GraphicsSettings.m_ShadowsResolution);
		m_DirectionalLightShadowPassPso->SetBias(m_GraphicsSettings.m_ShadowsDepthBias, m_GraphicsSettings.m_ShadowsNormalBias);
	}

	// Setup particles
	{
		m_ParticleSystem = std::make_unique<ParticleSystem>(device, *commandList, XMVectorSet(7.25f, 5.6f, 0.0f, 1.0f));
	}

	XMVECTOR lightDir = XMLoadFloat4(&m_DirectionalLight.m_DirectionWs);
	XMStoreFloat4(&m_DirectionalLight.m_DirectionWs, XMVector4Normalize(lightDir));

	ComPtr<ID3DBlob> vertexShaderBlob;
	ThrowIfFailed(D3DReadFileToBlob(L"LightingDemo_VertexShader.cso", &vertexShaderBlob));

	ComPtr<ID3DBlob> pixelShaderBlob;
	ThrowIfFailed(D3DReadFileToBlob(L"LightingDemo_PixelShader.cso", &pixelShaderBlob));

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

	CD3DX12_DESCRIPTOR_RANGE1 descriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, ModelMaps::TotalNumber, 0);
	CD3DX12_DESCRIPTOR_RANGE1 descriptorRangeShadowMaps(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, ModelMaps::TotalNumber + 1);

	CD3DX12_ROOT_PARAMETER1 rootParameters[RootParameters::NumRootParameters];
	rootParameters[RootParameters::MatricesCb].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE,
	                                                                    D3D12_SHADER_VISIBILITY_VERTEX);
	rootParameters[RootParameters::MaterialCb].InitAsConstantBufferView(0, 1, D3D12_ROOT_DESCRIPTOR_FLAG_NONE,
	                                                                    D3D12_SHADER_VISIBILITY_PIXEL);
	rootParameters[RootParameters::DirLightCb].InitAsConstantBufferView(1, 1, D3D12_ROOT_DESCRIPTOR_FLAG_NONE,
	                                                                    D3D12_SHADER_VISIBILITY_PIXEL);
	rootParameters[RootParameters::LightPropertiesCb].InitAsConstants(sizeof(LightPropertiesCb) / sizeof(float), 2, 0,
	                                                                  D3D12_SHADER_VISIBILITY_PIXEL);
	rootParameters[RootParameters::ShadowMatricesCb].InitAsConstantBufferView(1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE,
	                                                                          D3D12_SHADER_VISIBILITY_VERTEX);
	rootParameters[RootParameters::ShadowMaps].InitAsDescriptorTable(1, &descriptorRangeShadowMaps,
	                                                                 D3D12_SHADER_VISIBILITY_PIXEL);
	rootParameters[RootParameters::Textures].InitAsDescriptorTable(1, &descriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParameters[RootParameters::PointLights].InitAsShaderResourceView(
		ModelMaps::TotalNumber, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE,
		D3D12_SHADER_VISIBILITY_PIXEL);

	CD3DX12_STATIC_SAMPLER_DESC samplers[] = {
		// linear repeat
		CD3DX12_STATIC_SAMPLER_DESC(0, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR),
		// shadow map
		CD3DX12_STATIC_SAMPLER_DESC(1, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		                            D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 0, 16,
		                            D3D12_COMPARISON_FUNC_LESS_EQUAL),
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
		CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DsvFormat;
		CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RtvFormats;
		CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER Rasterizer;
	} pipelineStateStream;

	// sRGB formats provide free gamma correction!
	constexpr DXGI_FORMAT backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	constexpr DXGI_FORMAT depthBufferFormat = DXGI_FORMAT_D32_FLOAT;

	D3D12_RT_FORMAT_ARRAY rtvFormats = {};
	rtvFormats.NumRenderTargets = 1;
	rtvFormats.RTFormats[0] = backBufferFormat;

	pipelineStateStream.RootSignature = m_RootSignature.GetRootSignature().Get();
	pipelineStateStream.InputLayout = {VertexAttributes::INPUT_ELEMENTS, VertexAttributes::INPUT_ELEMENT_COUNT};
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

	ThrowIfFailed(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&m_PipelineState)));

	auto colorDesc = CD3DX12_RESOURCE_DESC::Tex2D(backBufferFormat,
	                                              m_Width, m_Height,
	                                              1, 1,
	                                              1, 0,
	                                              D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
	D3D12_CLEAR_VALUE colorClearValue;
	colorClearValue.Format = colorDesc.Format;
	memcpy(colorClearValue.Color, CLEAR_COLOR, sizeof CLEAR_COLOR);

	auto colorTexture = Texture(colorDesc, &colorClearValue,
	                            TextureUsageType::RenderTarget,
	                            L"Color Render Target");

	auto depthDesc = CD3DX12_RESOURCE_DESC::Tex2D(depthBufferFormat,
	                                              m_Width, m_Height,
	                                              1, 1,
	                                              1, 0,
	                                              D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
	D3D12_CLEAR_VALUE depthClearValue;
	depthClearValue.Format = depthDesc.Format;
	depthClearValue.DepthStencil = {1.0f, 0};

	auto depthTexture = Texture(depthDesc, &depthClearValue,
	                            TextureUsageType::Depth,
	                            L"Depth Render Target");

	m_RenderTarget.AttachTexture(Color0, colorTexture);
	m_RenderTarget.AttachTexture(DepthStencil, depthTexture);

	auto fenceValue = commandQueue->ExecuteCommandList(commandList);
	commandQueue->WaitForFenceValue(fenceValue);

	return true;
}

void LightingDemo::OnResize(ResizeEventArgs& e)
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

		m_RenderTarget.Resize(m_Width, m_Height);
	}
}

void LightingDemo::UnloadContent()
{
}

void LightingDemo::OnUpdate(UpdateEventArgs& e)
{
	static uint64_t frameCount = 0;
	static double totalTime = 0.0;

	Base::OnUpdate(e);

	if (e.ElapsedTime > 1.0f) return;

	totalTime += e.ElapsedTime;
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


	m_ParticleSystem->Update(e.ElapsedTime);

	auto dt = static_cast<float>(e.ElapsedTime);

	if (m_AnimateLights)
	{
		XMVECTOR dirLightDirectionWs = XMLoadFloat4(&m_DirectionalLight.m_DirectionWs);
		dirLightDirectionWs = XMVector4Transform(dirLightDirectionWs,
		                                         XMMatrixRotationAxis(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f),
		                                                              XMConvertToRadians(90.0f * dt)));
		XMStoreFloat4(&m_DirectionalLight.m_DirectionWs, dirLightDirectionWs);
	}
}

void LightingDemo::OnRender(RenderEventArgs& e)
{
	Base::OnRender(e);

	const auto commandQueue = Application::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	const auto commandList = commandQueue->GetCommandList();

	// Clear the render targets
	{
		commandList->ClearTexture(m_RenderTarget.GetTexture(Color0), CLEAR_COLOR);
		commandList->ClearDepthStencilTexture(m_RenderTarget.GetTexture(DepthStencil), D3D12_CLEAR_FLAG_DEPTH);
	}

	// Shadow pass
	{
		m_DirectionalLightShadowPassPso->ClearShadowMap(*commandList);
		m_DirectionalLightShadowPassPso->ComputePassParameters(m_Camera, m_DirectionalLight);
		m_DirectionalLightShadowPassPso->SetContext(*commandList);

		for (auto& gameObject : m_GameObjects)
		{
			m_DirectionalLightShadowPassPso->DrawToShadowMap(*commandList, gameObject);
		}
	}

	commandList->SetViewport(m_Viewport);
	commandList->SetScissorRect(m_ScissorRect);

	commandList->SetRenderTarget(m_RenderTarget);

	{
		const XMMATRIX viewMatrix = m_Camera.GetViewMatrix();
		const XMMATRIX projectionMatrix = m_Camera.GetProjectionMatrix();
		const XMMATRIX viewProjectionMatrix = viewMatrix * projectionMatrix;

		// Draw point lights
		{
			m_PointLightPso->Set(*commandList);

			for (const auto& pointLight : m_PointLights)
			{
				m_PointLightPso->Draw(*commandList, pointLight, viewMatrix, viewProjectionMatrix, projectionMatrix,
				                      1.0f);
			}
		}

		// Opaque pass
		{
			commandList->SetPipelineState(m_PipelineState);
			commandList->SetGraphicsRootSignature(m_RootSignature);

			// Update directional light
			{
				XMVECTOR lightDirVs = XMLoadFloat4(&m_DirectionalLight.m_DirectionWs);
				lightDirVs = XMVector4Transform(lightDirVs, viewMatrix);
				XMStoreFloat4(&m_DirectionalLight.m_DirectionVs, lightDirVs);

				DirectionalLightCb directionalLightCb;
				directionalLightCb.Color = m_DirectionalLight.m_Color;
				directionalLightCb.DirectionVs = m_DirectionalLight.m_DirectionVs;
				commandList->SetGraphicsDynamicConstantBuffer(RootParameters::DirLightCb, directionalLightCb);
			}

			// Update point lights
			{
				LightPropertiesCb lightPropertiesCb;
				lightPropertiesCb.NumPointLights = static_cast<uint32_t>(m_PointLights.size());

				for (auto& pointLight : m_PointLights)
				{
					XMVECTOR lightPosVs = XMLoadFloat4(&pointLight.m_PositionWs);
					lightPosVs = XMVector4Transform(lightPosVs, viewMatrix);
					XMStoreFloat4(&pointLight.m_PositionVs, lightPosVs);
				}

				commandList->SetGraphics32BitConstants(RootParameters::LightPropertiesCb, lightPropertiesCb);
				commandList->SetGraphicsDynamicStructuredBuffer(RootParameters::PointLights, m_PointLights);
			}

			// Bind shadow maps
			{
				m_DirectionalLightShadowPassPso->SetShadowMapShaderResourceView(
					*commandList, RootParameters::ShadowMaps);
			}

			ShadowMatricesCb shadowMatrices;
			shadowMatrices.ViewProjection = m_DirectionalLightShadowPassPso->GetShadowViewProjectionMatrix();
			commandList->SetGraphicsDynamicConstantBuffer(RootParameters::ShadowMatricesCb, shadowMatrices);

			for (const auto& go : m_GameObjects)
			{
				go.Draw([this, &viewMatrix, &viewProjectionMatrix, &projectionMatrix](auto& cmd, auto worldMatrix)
				        {
					        MatricesCb matrices;
					        matrices.Compute(worldMatrix, viewMatrix, viewProjectionMatrix, projectionMatrix);
					        cmd.SetGraphicsDynamicConstantBuffer(RootParameters::MatricesCb, matrices);
				        },
				        *commandList, RootParameters::MaterialCb, RootParameters::Textures);
			}
		}

		// Particle Systems
		{
			m_ParticleSystem->Draw(*commandList, viewMatrix, viewProjectionMatrix, projectionMatrix);
		}
	}

	commandQueue->ExecuteCommandList(commandList);
	PWindow->Present(m_RenderTarget.GetTexture(Color0));
}

void LightingDemo::OnKeyPressed(KeyEventArgs& e)
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
		m_GraphicsSettings.m_VSync = !m_GraphicsSettings.m_VSync;
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
	case KeyCode::L:
		m_AnimateLights = !m_AnimateLights;
		break;
	case KeyCode::ShiftKey:
		m_CameraController.m_Shift = true;
		break;
	}
}

void LightingDemo::OnKeyReleased(KeyEventArgs& e)
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
	}
}

void LightingDemo::OnMouseMoved(MouseMotionEventArgs& e)
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


void LightingDemo::OnMouseWheel(MouseWheelEventArgs& e)
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
