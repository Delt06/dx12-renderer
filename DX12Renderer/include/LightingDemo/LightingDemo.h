#pragma once

#include <DX12Library/Camera.h>
#include <DX12Library/Game.h>
#include <DX12Library/IndexBuffer.h>
#include <DX12Library/Window.h>

#include <DX12Library/RenderTarget.h>
#include <DX12Library/RootSignature.h>
#include <DX12Library/Texture.h>
#include <DX12Library/VertexBuffer.h>

#include <Light.h>
#include <Mesh.h>

#include "DirectionalLightShadowPassPso.h"
#include "../GameObject.h"
#include "../GraphicsSettings.h"
#include "ParticleSystemPso.h"
#include "PointLightPso.h"
#include "Scene.h"
#include "SpotLightShadowPassPso.h"
#include <DX12Library/Cubemap.h>

#include "PostFxPso.h"
#include "BloomPso.h"

class PointLightShadowPassPso;
class ParticleSystem;
class SceneRenderer;

class LightingDemo final : public Game
{
public:
	using Base = Game;

	LightingDemo(const std::wstring& name, int width, int height, GraphicsSettings graphicsSettings);
	~LightingDemo() override;

	bool LoadContent() override;
	void UnloadContent() override;

protected:
	void OnUpdate(UpdateEventArgs& e) override;
	void OnRender(RenderEventArgs& e) override;
	void OnKeyPressed(KeyEventArgs& e) override;
	void OnKeyReleased(KeyEventArgs& e) override;
	void OnMouseMoved(MouseMotionEventArgs& e) override;
	void OnMouseWheel(MouseWheelEventArgs& e) override;
	void OnResize(ResizeEventArgs& e) override;

private:
	std::shared_ptr<Texture> m_WhiteTexture2d;

	RenderTarget m_RenderTarget;
	RenderTarget m_PostFxRenderTarget;

	std::shared_ptr<Scene> m_Scene;
	std::shared_ptr<SceneRenderer> m_SceneRenderer;
	std::shared_ptr<Cubemap> m_ReflectionCubemap;
	std::unique_ptr<PostFxPso> m_PostFxPso;
	std::unique_ptr<BloomPso> m_BloomPso;

	D3D12_VIEWPORT m_Viewport;
	D3D12_RECT m_ScissorRect;

	bool m_AnimateLights = false;
	GraphicsSettings m_GraphicsSettings;

	static constexpr size_t ALIGNMENT = 16;

	struct alignas(ALIGNMENT) CameraData
	{
		DirectX::XMVECTOR m_InitialPosition;
		DirectX::XMVECTOR m_InitialQRotation;
	};

	CameraData* m_PAlignedCameraData;

	struct
	{
		float m_Forward;
		float m_Backward;
		float m_Left;
		float m_Right;
		float m_Up;
		float m_Down;

		float m_Pitch;
		float m_Yaw;

		bool m_Shift;
	} m_CameraController;

	int m_Width;
	int m_Height;
};
