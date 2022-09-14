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
#include <LightingDemo/SceneRenderer.h>

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

	bool allowFullscreenToggle = true;
	constexpr FLOAT CLEAR_COLOR[] = { 0.4f, 0.6f, 0.9f, 1.0f };

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
			// Texture2D register(t0-t1);
			Textures,
			NumRootParameters
		};
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

	constexpr DXGI_FORMAT gBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	constexpr DXGI_FORMAT depthBufferFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	D3D12_CLEAR_VALUE gBufferColorClearValue;
	gBufferColorClearValue.Format = gBufferFormat;
	memcpy(gBufferColorClearValue.Color, CLEAR_COLOR, sizeof CLEAR_COLOR);

	// create GBuffer root signature and pipeline state
	{
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

			CD3DX12_DESCRIPTOR_RANGE1 texturesDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0);

			CD3DX12_ROOT_PARAMETER1 rootParameters[GBufferRootParameters::NumRootParameters];
			rootParameters[GBufferRootParameters::MatricesCb].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX);
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
			rtvFormats.NumRenderTargets = 2;
			rtvFormats.RTFormats[0] = gBufferFormat;
			rtvFormats.RTFormats[1] = gBufferFormat;

			pipelineStateStream.RootSignature = m_GBufferPassRootSignature.GetRootSignature().Get();

			std::vector<D3D12_INPUT_ELEMENT_DESC> inputElements;
			inputElements.insert(inputElements.end(), std::begin(VertexAttributes::INPUT_ELEMENTS), std::end(VertexAttributes::INPUT_ELEMENTS));
			inputElements.insert(inputElements.end(), std::begin(SkinningVertexAttributes::INPUT_ELEMENTS), std::end(SkinningVertexAttributes::INPUT_ELEMENTS));

			pipelineStateStream.InputLayout = { inputElements.data(), static_cast<uint32_t>(inputElements.size()) };
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
	}

	// Generate default texture
	{
		m_WhiteTexture2d = std::make_shared<Texture>();
		commandList->LoadTextureFromFile(*m_WhiteTexture2d, L"Assets/Textures/white.png");
	}

	// Load models
	{
		ModelLoader modelLoader(m_WhiteTexture2d);

		{
			auto model = modelLoader.Load(*commandList, "Assets/Models/teapot/teapot.obj");
			{
				modelLoader.LoadMap(*model, *commandList, ModelMaps::Diffuse,
					L"Assets/Textures/PavingStones/PavingStones_1K_Color.jpg");
				modelLoader.LoadMap(*model, *commandList, ModelMaps::Normal,
					L"Assets/Textures/PavingStones/PavingStones_1K_Normal.jpg");
			}
			model->GetMaterial().SpecularPower = 50.0f;
			XMMATRIX translationMatrix = XMMatrixTranslation(0.0f, 0.0f, 0.0f);
			XMMATRIX rotationMatrix = XMMatrixIdentity();
			XMMATRIX scaleMatrix = XMMatrixScaling(0.1f, 0.1f, 0.1f);
			XMMATRIX worldMatrix = scaleMatrix * rotationMatrix * translationMatrix;
			m_GameObjects.push_back(GameObject(worldMatrix, model));
		}

		{
			auto model = modelLoader.LoadExisting(Mesh::CreatePlane(*commandList));
			{
				model->GetMaterial().SpecularPower = 10.0f;
				model->GetMaterial().TilingOffset = { 10, 10, 0, 0 };

				modelLoader.LoadMap(*model, *commandList, ModelMaps::Diffuse,
					L"Assets/Textures/Moss/Moss_1K_Color.jpg");
				modelLoader.LoadMap(*model, *commandList, ModelMaps::Normal,
					L"Assets/Textures/Moss/Moss_1K_Normal.jpg");
			}
			XMMATRIX translationMatrix = XMMatrixTranslation(0.0f, 0.0f, 0.0f);
			XMMATRIX rotationMatrix = XMMatrixIdentity();
			XMMATRIX scaleMatrix = XMMatrixScaling(200.0f, 200.0f, 200.0f);
			XMMATRIX worldMatrix = scaleMatrix * translationMatrix * rotationMatrix;
			m_GameObjects.push_back(GameObject(worldMatrix, model));
		}

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
			L"Diffuse Render Target");

		auto normalTexture = Texture(colorDesc, &gBufferColorClearValue,
			TextureUsageType::RenderTarget,
			L"Normal Render Target");

		auto depthDesc = CD3DX12_RESOURCE_DESC::Tex2D(depthBufferFormat,
			m_Width, m_Height,
			1, 1,
			1, 0,
			D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
		D3D12_CLEAR_VALUE depthClearValue;
		depthClearValue.Format = depthDesc.Format;
		depthClearValue.DepthStencil = { 1.0f, 0 };

		auto depthTexture = Texture(depthDesc, &depthClearValue,
			TextureUsageType::Depth,
			L"Depth Render Target");

		m_GBufferRenderTarget.AttachTexture(Color0, diffuseTexture);
		m_GBufferRenderTarget.AttachTexture(Color1, normalTexture);
		m_GBufferRenderTarget.AttachTexture(DepthStencil, depthTexture);
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
}

void DeferredLightingDemo::OnRender(RenderEventArgs& e)
{
	Base::OnRender(e);

	const auto commandQueue = Application::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	const auto commandList = commandQueue->GetCommandList();

	{
		PIXScope(*commandList, "Clear Render Targets");
		commandList->ClearRenderTarget(m_GBufferRenderTarget, CLEAR_COLOR, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL);
	}

	commandList->SetViewport(m_Viewport);
	commandList->SetScissorRect(m_ScissorRect);

	const XMMATRIX viewMatrix = m_Camera.GetViewMatrix();
	const XMMATRIX projectionMatrix = m_Camera.GetProjectionMatrix();
	const XMMATRIX viewProjection = viewMatrix * projectionMatrix;

	{
		PIXScope(*commandList, "GBuffer Pass");

		commandList->SetRenderTarget(m_GBufferRenderTarget);
		commandList->SetGraphicsRootSignature(m_GBufferPassRootSignature);
		commandList->SetPipelineState(m_GBufferPassPipelineState);

		for (const auto& go : m_GameObjects)
		{
			MatricesCb matricesCb;
			matricesCb.Compute(go.GetWorldMatrix(), viewMatrix, viewProjection, projectionMatrix);
			commandList->SetGraphicsDynamicConstantBuffer(GBufferRootParameters::MatricesCb, matricesCb);

			const auto& model = go.GetModel();
			std::shared_ptr<Texture> maps[ModelMaps::TotalNumber];
			model->GetMaps(maps);
			commandList->SetShaderResourceView(GBufferRootParameters::Textures, 0, *maps[ModelMaps::Diffuse], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			commandList->SetShaderResourceView(GBufferRootParameters::Textures, 1, *maps[ModelMaps::Normal], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

			for (const auto& mesh : model->GetMeshes())
			{
				mesh->Draw(*commandList);
			}
		}
	}

	commandQueue->ExecuteCommandList(commandList);
	PWindow->Present(m_GBufferRenderTarget.GetTexture(Color0));
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
