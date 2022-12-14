#include <AnimationsDemo.h>

#include <DX12Library/Application.h>
#include <DX12Library/CommandQueue.h>
#include <DX12Library/CommandList.h>
#include <DX12Library/Helpers.h>
#include <DX12Library/Window.h>
#include <Framework/Bone.h>
#include <Framework/Animation.h>
#include <Framework/GameObject.h>
#include <Framework/Light.h>

#include <wrl.h>

#include <Framework/GraphicsSettings.h>
#include <Framework/Model.h>
#include <Framework/ModelLoader.h>

using namespace Microsoft::WRL;

#include <d3d12.h>
#include <d3dx12.h>
#include <DirectXColors.h>

using namespace DirectX;

#include <algorithm> // For std::min and std::max.
#if defined(min)
#undef min
#endif

#include <DX12Library/ShaderUtils.h>

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

	namespace CBuffer
	{
		struct Model
		{
			XMMATRIX m_ModelViewProjection;
			XMMATRIX m_InverseTransposeModel;

			void Compute(const XMMATRIX& model, const XMMATRIX& viewProjection)
			{
				m_ModelViewProjection = model * viewProjection;
				m_InverseTransposeModel = XMMatrixTranspose(XMMatrixInverse(nullptr, model));
			}
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

	// Generate default texture
	{
		m_WhiteTexture2d = std::make_shared<Texture>();
		commandList->LoadTextureFromFile(*m_WhiteTexture2d, L"Assets/Textures/white.png");
	}

	m_RootSignature = std::make_shared<CommonRootSignature>(m_WhiteTexture2d);

	m_BonesStructuredBuffer = std::make_shared<StructuredBuffer>(L"Bones Structured Buffer");

	auto modelShader = std::make_shared<Shader>(m_RootSignature,
		ShaderBlob(L"AnimationsDemo_VS.cso"),
		ShaderBlob(L"AnimationsDemo_PS.cso"),
		[](PipelineStateBuilder& builder)
		{
			std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout;
			inputLayout.insert(inputLayout.end(), std::begin(VertexAttributes::INPUT_ELEMENTS), std::end(VertexAttributes::INPUT_ELEMENTS));
			inputLayout.insert(inputLayout.end(), std::begin(SkinningVertexAttributes::INPUT_ELEMENTS), std::end(SkinningVertexAttributes::INPUT_ELEMENTS));
			builder.WithInputLayout(inputLayout);
		}
	);

	// bones
	{
		auto shader = std::make_shared<Shader>(m_RootSignature,
			ShaderBlob(L"AnimationsDemo_Bone_VS.cso"),
			ShaderBlob(L"AnimationsDemo_Bone_PS.cso"),
			[](PipelineStateBuilder& builder)
			{
				builder
					.WithRasterizer(CD3DX12_RASTERIZER_DESC(D3D12_FILL_MODE_WIREFRAME, D3D12_CULL_MODE_BACK, FALSE, 0, 0,
						0, TRUE, FALSE, FALSE, 0,
						D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF))
					.WithDisabledDepthStencil()
					;
			}
		);
		m_BoneMaterial = Material::Create(shader);
	}

	// Load models and animations
	{
		ModelLoader modelLoader;

		const auto MaterialSetTexture = [&modelLoader, &commandList](Material& material, const std::string& propertyName, const std::wstring& texturePath, TextureUsageType usage = TextureUsageType::Albedo)
		{
			material.SetShaderResourceView(propertyName, ShaderResourceView(modelLoader.LoadTexture(*commandList, texturePath, usage)));
		};

		{
			auto model = modelLoader.Load(*commandList, "Assets/Models/archer/archer.fbx");
			auto material = Material::Create(modelShader);
			MaterialSetTexture(*material, "diffuseMap", L"Assets/Models/archer/textures/akai_diffuse.png");

			XMMATRIX translationMatrix = XMMatrixTranslation(0.0f, 0.0f, 0.0f);
			XMMATRIX rotationMatrix = XMMatrixIdentity();
			XMMATRIX scaleMatrix = XMMatrixScaling(0.05f, 0.05f, 0.05f);
			XMMATRIX worldMatrix = scaleMatrix * rotationMatrix * translationMatrix;
			m_GameObjects.push_back(GameObject(worldMatrix, model, material));
		}

		m_BoneMesh = Mesh::CreateCube(*commandList);

		{
			m_RunAnimation = modelLoader.LoadAnimation("Assets/Models/archer/fast_run.fbx", "mixamo.com");
			m_IdleAnimation = modelLoader.LoadAnimation("Assets/Models/archer/idle.fbx", "mixamo.com");
			m_TopAnimation = modelLoader.LoadAnimation("Assets/Models/archer/reaction.fbx", "mixamo.com");
		}
	}

	auto colorDesc = CD3DX12_RESOURCE_DESC::Tex2D(backBufferFormat,
		m_Width, m_Height,
		1, 1,
		1, 0,
		D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

	auto colorTexture = std::make_shared<Texture>(colorDesc, &colorClearValue,
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

	auto depthTexture = std::make_shared<Texture>(depthDesc, &depthClearValue,
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
			const auto runTransforms = m_RunAnimation->GetBonesTranforms(*mesh, m_Time);
			const auto idleTransforms = m_IdleAnimation->GetBonesTranforms(*mesh, m_Time);
			const auto topTransforms = m_TopAnimation->GetBonesTranforms(*mesh, m_Time);
			const auto topBodyMask = Animation::BuildMask(*mesh, "mixamorig:Spine");

			auto transforms = Animation::Blend(runTransforms, idleTransforms, weight);
			transforms = Animation::ApplyMask(topTransforms, transforms, topBodyMask);

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
		commandList->ClearTexture(*m_RenderTarget.GetTexture(Color0), CLEAR_COLOR);
		commandList->ClearDepthStencilTexture(*m_RenderTarget.GetTexture(DepthStencil), D3D12_CLEAR_FLAG_DEPTH);
	}

	commandList->SetRenderTarget(m_RenderTarget);
	commandList->SetViewport(m_Viewport);
	commandList->SetScissorRect(m_ScissorRect);

	const XMMATRIX viewMatrix = m_Camera.GetViewMatrix();
	const XMMATRIX projectionMatrix = m_Camera.GetProjectionMatrix();
	const XMMATRIX viewProjection = viewMatrix * projectionMatrix;

	m_RootSignature->Bind(*commandList);

	{
		PIXScope(*commandList, "Main Pass");

		for (const auto& go : m_GameObjects)
		{
			CBuffer::Model modelCBuffer{};
			modelCBuffer.Compute(go.GetWorldMatrix(), viewProjection);
			m_RootSignature->SetModelConstantBuffer(*commandList, modelCBuffer);

			const auto& model = go.GetModel();
			const auto& material = go.GetMaterial();
			material->BeginBatch(*commandList);
			material->UploadUniforms(*commandList);

			for (const auto& mesh : model->GetMeshes())
			{
				auto& bones = mesh->GetBones();
				std::vector<BoneSbItem> bonesSb;
				bonesSb.reserve(bones.size());

				for (const auto& bone : bones)
				{
					bonesSb.push_back({ bone.Offset * bone.GlobalTransform });
				}

				commandList->CopyStructuredBuffer(*m_BonesStructuredBuffer, bonesSb);
				m_RootSignature->SetMaterialShaderResourceView(*commandList, 0, ShaderResourceView(m_BonesStructuredBuffer));
				mesh->Draw(*commandList);
			}

			material->EndBatch(*commandList);
		}
	}

	{
		PIXScope(*commandList, "Draw Bones");

		for (const auto& go : m_GameObjects)
		{
			const auto& model = go.GetModel();


			const auto& material = m_BoneMaterial;
			material->BeginBatch(*commandList);
			material->UploadUniforms(*commandList);

			for (const auto& mesh : model->GetMeshes())
			{
				for (const auto& bone : mesh->GetBones())
				{
					CBuffer::Model modelCBuffer{};
					XMMATRIX boneModelMatrix = bone.GlobalTransform * go.GetWorldMatrix();
					modelCBuffer.Compute(boneModelMatrix, viewProjection);
					m_RootSignature->SetModelConstantBuffer(*commandList, modelCBuffer);
					m_BoneMesh->Draw(*commandList);
				}
			}

			material->EndBatch(*commandList);
		}
	}

	commandQueue->ExecuteCommandList(commandList);
	PWindow->Present(*m_RenderTarget.GetTexture(Color0));
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
