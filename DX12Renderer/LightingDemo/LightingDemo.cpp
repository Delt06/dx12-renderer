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

#include "ModelLoader.h"
using namespace Microsoft::WRL;

#include <d3d12.h>
#include <d3dx12.h>
#include <d3dcompiler.h>
#include <DirectXColors.h>

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
	struct Matrices
	{
		XMMATRIX Model;
		XMMATRIX ModelView;
		XMMATRIX InverseTransposeModelView;
		XMMATRIX ModelViewProjection;
	};

	struct DirectionalLightCb
	{
		XMFLOAT4 DirectionVs;
		XMFLOAT4 Color;
	};

	// An enum for root signature parameters.
	// I'm not using scoped enums to avoid the explicit cast that would be required
	// to use these as root indices in the root signature.
	enum RootParameters
	{
		// ConstantBuffer<Matrices> MatCB : register(b0);
		MatricesCb,
		// ConstantBuffer<Material> MaterialCB : register( b0, space1 );
		MaterialCb,
		// ConstantBuffer<Material> DirLightCb : register( b1, space1 );
		DirLightCb,
		Textures,
		NumRootParameters
	};

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


LightingDemo::LightingDemo(const std::wstring& name, int width, int height, bool vSync)
	: Base(name, width, height, vSync)
	  , Viewport(CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)))
	  , ScissorRect(CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX))
	  , CameraController{}
	  , AnimatedLights(false)
	  , Width(0)
	  , Height(0)
{
	XMVECTOR cameraPos = XMVectorSet(0, 5, -20, 1);
	XMVECTOR cameraTarget = XMVectorSet(0, 5, 0, 1);
	XMVECTOR cameraUp = XMVectorSet(0, 1, 0, 0);

	MCamera.SetLookAt(cameraPos, cameraTarget, cameraUp);

	PAlignedCameraData = static_cast<CameraData*>(_aligned_malloc(sizeof(CameraData), 16));

	PAlignedCameraData->InitialPosition = MCamera.GetTranslation();
	PAlignedCameraData->InitialQRotation = MCamera.GetRotation();
}

LightingDemo::~LightingDemo()
{
	_aligned_free(PAlignedCameraData);
}

bool LightingDemo::LoadContent()
{
	const auto device = Application::Get().GetDevice();
	const auto commandQueue = Application::Get().GetCommandQueue();
	const auto commandList = commandQueue->GetCommandList();

	for (auto& mesh : ModelLoader::LoadObj(*commandList, "Assets/Models/teapot/teapot.obj", true))
	{
		XMMATRIX translationMatrix = XMMatrixTranslation(0.0f, 0.0f, 0.0f);
		XMMATRIX rotationMatrix = XMMatrixIdentity();
		XMMATRIX scaleMatrix = XMMatrixScaling(0.1f, 0.1f, 0.1f);
		XMMATRIX worldMatrix = scaleMatrix * rotationMatrix * translationMatrix;
		m_GameObjects.push_back(GameObject(worldMatrix, mesh));
	}

	for (auto& mesh : ModelLoader::LoadObj(*commandList, "Assets/Models/cube/cube.obj", true))
	{
		XMMATRIX translationMatrix = XMMatrixTranslation(0.0f, 0.0f, 0.0f);
		XMMATRIX rotationMatrix = XMMatrixIdentity();
		XMMATRIX scaleMatrix = XMMatrixScaling(1.0f, 1.0f, 1.0f);
		XMMATRIX worldMatrix = scaleMatrix * rotationMatrix * translationMatrix;
		m_GameObjects.push_back(GameObject(worldMatrix, mesh));
	}

	for (auto& mesh : ModelLoader::LoadObj(*commandList, "Assets/Models/sphere/sphere-cylcoords-1k.obj", true))
	{
		XMMATRIX translationMatrix = XMMatrixTranslation(0.0f, 0.0f, 50.0f);
		XMMATRIX rotationMatrix = XMMatrixIdentity();
		XMMATRIX scaleMatrix = XMMatrixScaling(0.1f, 0.1f, 0.1f);
		XMMATRIX worldMatrix = scaleMatrix * rotationMatrix * translationMatrix;
		m_GameObjects.push_back(GameObject(worldMatrix, mesh));
	}
	
	{
		auto mesh = Mesh::CreatePlane(*commandList);
		XMMATRIX translationMatrix = XMMatrixTranslation(0.0f, 0.0f, 0.0f);
		XMMATRIX rotationMatrix = XMMatrixIdentity();
		XMMATRIX scaleMatrix = XMMatrixScaling(20.0f, 1.0f, 20.0f);
		XMMATRIX worldMatrix = scaleMatrix * translationMatrix * rotationMatrix;
		m_GameObjects.push_back(GameObject(worldMatrix, mesh));
	}

	m_Texture = std::make_shared<Texture>();
	commandList->LoadTextureFromFile(*m_Texture, L"Assets/Textures/bricks.jpg");

	MDirectionalLight.DirectionWs = XMFLOAT4(1.0f, 1.0f, 0.0f, 0.0f);

	XMVECTOR lightDir = XMLoadFloat4(&MDirectionalLight.DirectionWs);
	XMStoreFloat4(&MDirectionalLight.DirectionWs, XMVector4Normalize(lightDir));

	ComPtr<ID3DBlob> vertexShaderBlob;
	ThrowIfFailed(D3DReadFileToBlob(L"Tutorial3_VertexShader.cso", &vertexShaderBlob));

	ComPtr<ID3DBlob> pixelShaderBlob;
	ThrowIfFailed(D3DReadFileToBlob(L"Tutorial3_PixelShader.cso", &pixelShaderBlob));

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

	CD3DX12_DESCRIPTOR_RANGE1 descriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	CD3DX12_ROOT_PARAMETER1 rootParameters[NumRootParameters];
	rootParameters[MatricesCb].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE,
	                                                    D3D12_SHADER_VISIBILITY_VERTEX);
	rootParameters[MaterialCb].InitAsConstantBufferView(0, 1, D3D12_ROOT_DESCRIPTOR_FLAG_NONE,
	                                                    D3D12_SHADER_VISIBILITY_PIXEL);
	rootParameters[DirLightCb].InitAsConstantBufferView(1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE,
	                                                    D3D12_SHADER_VISIBILITY_PIXEL);
	rootParameters[Textures].InitAsDescriptorTable(1, &descriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);

	CD3DX12_STATIC_SAMPLER_DESC linearRepeatSampler(0, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR);

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
	rootSignatureDescription.Init_1_1(NumRootParameters, rootParameters, 1, &linearRepeatSampler, rootSignatureFlags);

	MRootSignature.SetRootSignatureDesc(rootSignatureDescription.Desc_1_1, featureData.HighestVersion);

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

	pipelineStateStream.RootSignature = MRootSignature.GetRootSignature().Get();
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

	ThrowIfFailed(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&MPipelineState)));

	auto colorDesc = CD3DX12_RESOURCE_DESC::Tex2D(backBufferFormat,
	                                              Width, Height,
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
	                                              Width, Height,
	                                              1, 1,
	                                              1, 0,
	                                              D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
	D3D12_CLEAR_VALUE depthClearValue;
	depthClearValue.Format = depthDesc.Format;
	depthClearValue.DepthStencil = {1.0f, 0};

	auto depthTexture = Texture(depthDesc, &depthClearValue,
	                            TextureUsageType::Depth,
	                            L"Depth Render Target");

	MRenderTarget.AttachTexture(Color0, colorTexture);
	MRenderTarget.AttachTexture(DepthStencil, depthTexture);

	auto fenceValue = commandQueue->ExecuteCommandList(commandList);
	commandQueue->WaitForFenceValue(fenceValue);

	return true;
}

void LightingDemo::OnResize(ResizeEventArgs& e)
{
	Base::OnResize(e);

	if (Width != e.Width || Height != e.Height)
	{
		Width = std::max(1, e.Width);
		Height = std::max(1, e.Height);

		const float aspectRatio = static_cast<float>(Width) / static_cast<float>(Height);
		MCamera.SetProjection(45.0f, aspectRatio, 0.1f, 1000.0f);

		Viewport = CD3DX12_VIEWPORT(0.0f, 0.0f,
		                            static_cast<float>(Width), static_cast<float>(Height));

		MRenderTarget.Resize(Width, Height);
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
	float speedMultiplier = (CameraController.Shift ? 16.0f : 4.0f);

	XMVECTOR cameraTranslate = XMVectorSet(CameraController.Right - CameraController.Left, 0.0f,
	                                       CameraController.Forward - CameraController.Backward, 1.0f) * speedMultiplier
		* static_cast<float>(e.ElapsedTime);
	XMVECTOR cameraPan = XMVectorSet(0.0f, CameraController.Up - CameraController.Down, 0.0f, 1.0f) * speedMultiplier *
		static_cast<float>(e.ElapsedTime);
	MCamera.Translate(cameraTranslate, Space::Local);
	MCamera.Translate(cameraPan, Space::Local);

	XMVECTOR cameraRotation = XMQuaternionRotationRollPitchYaw(XMConvertToRadians(CameraController.Pitch),
	                                                           XMConvertToRadians(CameraController.Yaw), 0.0f);
	MCamera.SetRotation(cameraRotation);
}

void XM_CALLCONV ComputeMatrices(FXMMATRIX model, CXMMATRIX view, CXMMATRIX viewProjection, Matrices& matrices)
{
	matrices.Model = model;
	matrices.ModelView = model * view;
	matrices.InverseTransposeModelView = XMMatrixTranspose(XMMatrixInverse(nullptr, matrices.ModelView));
	matrices.ModelViewProjection = model * viewProjection;
}

void LightingDemo::OnRender(RenderEventArgs& e)
{
	Base::OnRender(e);

	const auto commandQueue = Application::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	const auto commandList = commandQueue->GetCommandList();

	// Clear the render targets
	{
		commandList->ClearTexture(MRenderTarget.GetTexture(Color0), CLEAR_COLOR);
		commandList->ClearDepthStencilTexture(MRenderTarget.GetTexture(DepthStencil), D3D12_CLEAR_FLAG_DEPTH);
	}

	commandList->SetPipelineState(MPipelineState);
	commandList->SetGraphicsRootSignature(MRootSignature);

	commandList->SetViewport(Viewport);
	commandList->SetScissorRect(ScissorRect);

	commandList->SetRenderTarget(MRenderTarget);

	{
		const XMMATRIX viewMatrix = MCamera.GetViewMatrix();
		const XMMATRIX viewProjectionMatrix = viewMatrix * MCamera.GetProjectionMatrix();

		XMVECTOR lightDirVs = XMLoadFloat4(&MDirectionalLight.DirectionWs);
		lightDirVs = XMVector4Transform(lightDirVs, MCamera.GetViewMatrix());
		XMStoreFloat4(&MDirectionalLight.DirectionVs, lightDirVs);

		DirectionalLightCb directionalLightCb;
		directionalLightCb.Color = MDirectionalLight.Color;
		directionalLightCb.DirectionVs = MDirectionalLight.DirectionVs;

		commandList->SetGraphicsDynamicConstantBuffer(MaterialCb, Material());
		commandList->SetGraphicsDynamicConstantBuffer(DirLightCb, directionalLightCb);
		commandList->SetShaderResourceView(Textures, 0, *m_Texture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		for (const auto& go : m_GameObjects)
		{
			Matrices matrices;
			ComputeMatrices(go.m_WorldMatrix, viewMatrix, viewProjectionMatrix, matrices);
			commandList->SetGraphicsDynamicConstantBuffer(MatricesCb, matrices);
			go.m_Mesh->Draw(*commandList);
		}
	}

	commandQueue->ExecuteCommandList(commandList);
	PWindow->Present(MRenderTarget.GetTexture(Color0));
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
		break;
	case KeyCode::R:
		// Reset camera transform
		MCamera.SetTranslation(PAlignedCameraData->InitialPosition);
		MCamera.SetRotation(PAlignedCameraData->InitialQRotation);
		CameraController.Pitch = 0.0f;
		CameraController.Yaw = 0.0f;
		break;
	case KeyCode::Up:
	case KeyCode::W:
		CameraController.Forward = 1.0f;
		break;
	case KeyCode::Left:
	case KeyCode::A:
		CameraController.Left = 1.0f;
		break;
	case KeyCode::Down:
	case KeyCode::S:
		CameraController.Backward = 1.0f;
		break;
	case KeyCode::Right:
	case KeyCode::D:
		CameraController.Right = 1.0f;
		break;
	case KeyCode::Q:
		CameraController.Down = 1.0f;
		break;
	case KeyCode::E:
		CameraController.Up = 1.0f;
		break;
	case KeyCode::Space:
		AnimatedLights = !AnimatedLights;
		break;
	case KeyCode::ShiftKey:
		CameraController.Shift = true;
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
		CameraController.Forward = 0.0f;
		break;
	case KeyCode::Left:
	case KeyCode::A:
		CameraController.Left = 0.0f;
		break;
	case KeyCode::Down:
	case KeyCode::S:
		CameraController.Backward = 0.0f;
		break;
	case KeyCode::Right:
	case KeyCode::D:
		CameraController.Right = 0.0f;
		break;
	case KeyCode::Q:
		CameraController.Down = 0.0f;
		break;
	case KeyCode::E:
		CameraController.Up = 0.0f;
		break;
	case KeyCode::ShiftKey:
		CameraController.Shift = false;
		break;
	}
}

void LightingDemo::OnMouseMoved(MouseMotionEventArgs& e)
{
	Base::OnMouseMoved(e);

	const float mouseSpeed = 0.1f;

	if (e.LeftButton)
	{
		CameraController.Pitch -= e.RelY * mouseSpeed;

		CameraController.Pitch = Clamp(CameraController.Pitch, -90.0f, 90.0f);

		CameraController.Yaw -= e.RelX * mouseSpeed;
	}
}


void LightingDemo::OnMouseWheel(MouseWheelEventArgs& e)
{
	{
		auto fov = MCamera.GetFov();

		fov -= e.WheelDelta;
		fov = Clamp(fov, 12.0f, 90.0f);

		MCamera.SetFov(fov);

		char buffer[256];
		sprintf_s(buffer, "FoV: %f\n", fov);
		OutputDebugStringA(buffer);
	}
}
