#include "AnimationsDemo.h"

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

	struct BonesPropertiesCb
	{
		uint32_t NumBones;
	};

	struct BoneSbItem
	{
		XMMATRIX Transform;
	};

	namespace RootParameters
	{
		enum RootParameters
		{
			// ConstantBuffer : register(b0);
			MatricesCb,
			// ConstantBuffer : register(b1);
			BonesPropertiesCb,
			// StructuredBuffer : register(t0);
			Bones,
			// Texture2D register(t0, space1);
			Diffuse,
			NumRootParameters
		};
	}

	namespace RootParametersBones
	{
		enum RootParametersBones
		{
			// ConstantBuffer register(b0);
			MatricesCb,
			NumRootParameters
		};
	}
}


AnimationsDemo::AnimationsDemo(const std::wstring& name, int width, int height, GraphicsSettings graphicsSettings)
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

AnimationsDemo::~AnimationsDemo()
{
	_aligned_free(m_PAlignedCameraData);
}

bool AnimationsDemo::LoadContent()
{
	const auto device = Application::Get().GetDevice();
	const auto commandQueue = Application::Get().GetCommandQueue();
	const auto commandList = commandQueue->GetCommandList();

	// sRGB formats provide free gamma correction!
	constexpr DXGI_FORMAT backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	constexpr DXGI_FORMAT depthBufferFormat = DXGI_FORMAT_D32_FLOAT;
	D3D12_CLEAR_VALUE colorClearValue;
	colorClearValue.Format = backBufferFormat;
	memcpy(colorClearValue.Color, CLEAR_COLOR, sizeof CLEAR_COLOR);

	// create root signature and pipeline state
	{
		{
			ComPtr<ID3DBlob> vertexShaderBlob;
			ThrowIfFailed(D3DReadFileToBlob(L"AnimationsDemo_VertexShader.cso", &vertexShaderBlob));

			ComPtr<ID3DBlob> pixelShaderBlob;
			ThrowIfFailed(D3DReadFileToBlob(L"AnimationsDemo_PixelShader.cso", &pixelShaderBlob));

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

			CD3DX12_DESCRIPTOR_RANGE1 descriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 1);

			CD3DX12_ROOT_PARAMETER1 rootParameters[RootParameters::NumRootParameters];
			rootParameters[RootParameters::MatricesCb].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX);
			rootParameters[RootParameters::BonesPropertiesCb].InitAsConstants(sizeof(BonesPropertiesCb) / sizeof(float), 1, 0, D3D12_SHADER_VISIBILITY_VERTEX);
			rootParameters[RootParameters::Bones].InitAsShaderResourceView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX);
			rootParameters[RootParameters::Diffuse].InitAsDescriptorTable(1, &descriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);

			CD3DX12_STATIC_SAMPLER_DESC samplers[] = {
				// linear repeat
				CD3DX12_STATIC_SAMPLER_DESC(0, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR),
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

			D3D12_RT_FORMAT_ARRAY rtvFormats = {};
			rtvFormats.NumRenderTargets = 1;
			rtvFormats.RTFormats[0] = backBufferFormat;

			pipelineStateStream.RootSignature = m_RootSignature.GetRootSignature().Get();

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

			ThrowIfFailed(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&m_PipelineState)));
		}
	}

	// create root signature and pipeline state for bones
	{
		{
			ComPtr<ID3DBlob> vertexShaderBlob;
			ThrowIfFailed(D3DReadFileToBlob(L"AnimationsDemo_Bone_VertexShader.cso", &vertexShaderBlob));

			ComPtr<ID3DBlob> pixelShaderBlob;
			ThrowIfFailed(D3DReadFileToBlob(L"AnimationsDemo_Bone_PixelShader.cso", &pixelShaderBlob));

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

			CD3DX12_DESCRIPTOR_RANGE1 descriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

			CD3DX12_ROOT_PARAMETER1 rootParameters[RootParametersBones::NumRootParameters];
			rootParameters[RootParametersBones::MatricesCb].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX);

			CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
			rootSignatureDescription.Init_1_1(RootParametersBones::NumRootParameters, rootParameters, 0, nullptr,
				rootSignatureFlags);

			m_BonesRootSignature.SetRootSignatureDesc(rootSignatureDescription.Desc_1_1, featureData.HighestVersion);

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
			rtvFormats.RTFormats[0] = backBufferFormat;

			pipelineStateStream.RootSignature = m_BonesRootSignature.GetRootSignature().Get();
			pipelineStateStream.InputLayout = { VertexAttributes::INPUT_ELEMENTS, VertexAttributes::INPUT_ELEMENT_COUNT };
			pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			pipelineStateStream.Vs = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
			pipelineStateStream.Ps = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
			pipelineStateStream.DsvFormat = depthBufferFormat;
			pipelineStateStream.RtvFormats = rtvFormats;
			pipelineStateStream.Rasterizer = CD3DX12_RASTERIZER_DESC(D3D12_FILL_MODE_WIREFRAME, D3D12_CULL_MODE_BACK, FALSE, 0, 0,
				0, TRUE, FALSE, FALSE, 0,
				D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF);

			const D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
				sizeof(PipelineStateStream), &pipelineStateStream
			};

			pipelineStateStream.DepthStencil = CD3DX12_DEPTH_STENCIL_DESC();

			ThrowIfFailed(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&m_BonesPipelineState)));
		}
	}

	// Generate default texture
	{
		m_WhiteTexture2d = std::make_shared<Texture>();
		commandList->LoadTextureFromFile(*m_WhiteTexture2d, L"Assets/Textures/white.png");
	}

	// Load models and animations
	{
		ModelLoader modelLoader(m_WhiteTexture2d);

		{
			auto model = modelLoader.Load(*commandList, "Assets/Models/archer/archer.fbx");
			{

			}

			XMMATRIX translationMatrix = XMMatrixTranslation(0.0f, 0.0f, 0.0f);
			XMMATRIX rotationMatrix = XMMatrixIdentity();
			XMMATRIX scaleMatrix = XMMatrixScaling(0.05f, 0.05f, 0.05f);
			XMMATRIX worldMatrix = scaleMatrix * rotationMatrix * translationMatrix;
			m_GameObjects.push_back(GameObject(worldMatrix, model));
		}

		m_BoneMesh = Mesh::CreateCube(*commandList);

		{
			m_RunAnimation = modelLoader.LoadAnimation("Assets/Models/archer/fast_run.fbx", "mixamo.com");
			m_IdleAnimation = modelLoader.LoadAnimation("Assets/Models/archer/idle.fbx", "mixamo.com");
		}
	}

	auto colorDesc = CD3DX12_RESOURCE_DESC::Tex2D(backBufferFormat,
		m_Width, m_Height,
		1, 1,
		1, 0,
		D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

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
	depthClearValue.DepthStencil = { 1.0f, 0 };

	auto depthTexture = Texture(depthDesc, &depthClearValue,
		TextureUsageType::Depth,
		L"Depth Render Target");

	m_RenderTarget.AttachTexture(Color0, colorTexture);
	m_RenderTarget.AttachTexture(DepthStencil, depthTexture);

	auto fenceValue = commandQueue->ExecuteCommandList(commandList);
	commandQueue->WaitForFenceValue(fenceValue);

	return true;
}

void AnimationsDemo::OnResize(ResizeEventArgs& e)
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

void AnimationsDemo::UnloadContent()
{
}

void AnimationsDemo::OnUpdate(UpdateEventArgs& e)
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

	// oscillate between 0 and 1 while spending some time on exact values of 0 and 1
	auto weight = static_cast<float>(Smoothstep(0.25, 0.75, (sin(m_Time * 2) + 1) * 0.5));

	for (const auto& go : m_GameObjects)
	{
		for (auto& mesh : go.GetModel()->GetMeshes())
		{
			auto runTransforms = m_RunAnimation->GetBonesTranforms(*mesh, m_Time);
			auto idleTransforms = m_IdleAnimation->GetBonesTranforms(*mesh, m_Time);
			auto transforms = Animation::Blend(runTransforms, idleTransforms, weight);
			Animation::Apply(*mesh, transforms);
			mesh->UpdateBoneGlobalTransforms();
		}
	}
}

void AnimationsDemo::OnRender(RenderEventArgs& e)
{
	Base::OnRender(e);

	const auto commandQueue = Application::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	const auto commandList = commandQueue->GetCommandList();

	// Clear the render targets
	{
		commandList->ClearTexture(m_RenderTarget.GetTexture(Color0), CLEAR_COLOR);
		commandList->ClearDepthStencilTexture(m_RenderTarget.GetTexture(DepthStencil), D3D12_CLEAR_FLAG_DEPTH);
	}

	commandList->SetRenderTarget(m_RenderTarget);
	commandList->SetViewport(m_Viewport);
	commandList->SetScissorRect(m_ScissorRect);

	const XMMATRIX viewMatrix = m_Camera.GetViewMatrix();
	const XMMATRIX projectionMatrix = m_Camera.GetProjectionMatrix();
	const XMMATRIX viewProjection = viewMatrix * projectionMatrix;

	{
		PIXScope(*commandList, "Main Pass");

		commandList->SetGraphicsRootSignature(m_RootSignature);
		commandList->SetPipelineState(m_PipelineState);

		for (const auto& go : m_GameObjects)
		{
			MatricesCb matricesCb;
			matricesCb.Compute(go.GetWorldMatrix(), viewMatrix, viewProjection, projectionMatrix);
			commandList->SetGraphicsDynamicConstantBuffer(RootParameters::MatricesCb, matricesCb);

			const auto& model = go.GetModel();
			std::shared_ptr<Texture> maps[ModelMaps::TotalNumber];
			model->GetMaps(maps);
			commandList->SetShaderResourceView(RootParameters::Diffuse, 0, *maps[ModelMaps::Diffuse], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

			for (const auto& mesh : model->GetMeshes())
			{
				BonesPropertiesCb bonesPropertiesCb;
				auto& bones = mesh->GetBones();
				bonesPropertiesCb.NumBones = bones.size();
				commandList->SetGraphics32BitConstants(RootParameters::BonesPropertiesCb, bonesPropertiesCb);

				std::vector<BoneSbItem> bonesSb;
				bonesSb.reserve(bones.size());

				for (const auto& bone : bones)
				{
					bonesSb.push_back({ bone.Offset * bone.GlobalTransform });
				}

				commandList->SetGraphicsDynamicStructuredBuffer(RootParameters::Bones, bonesSb);

				mesh->Draw(*commandList);
			}
		}
	}

	{
		PIXScope(*commandList, "Draw Bones");

		commandList->SetGraphicsRootSignature(m_BonesRootSignature);
		commandList->SetPipelineState(m_BonesPipelineState);

		for (const auto& go : m_GameObjects)
		{
			const auto& model = go.GetModel();

			for (const auto& mesh : model->GetMeshes())
			{
				for (const auto& bone : mesh->GetBones())
				{
					MatricesCb matricesCb;
					XMMATRIX worldMatrix = bone.GlobalTransform * go.GetWorldMatrix();
					matricesCb.Compute(worldMatrix, viewMatrix, viewProjection, projectionMatrix);
					commandList->SetGraphicsDynamicConstantBuffer(RootParametersBones::MatricesCb, matricesCb);

					m_BoneMesh->Draw(*commandList);
				}
			}
		}
	}

	commandQueue->ExecuteCommandList(commandList);
	PWindow->Present(m_RenderTarget.GetTexture(Color0));
}

void AnimationsDemo::OnKeyPressed(KeyEventArgs& e)
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

void AnimationsDemo::OnKeyReleased(KeyEventArgs& e)
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

void AnimationsDemo::OnMouseMoved(MouseMotionEventArgs& e)
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


void AnimationsDemo::OnMouseWheel(MouseWheelEventArgs& e)
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
