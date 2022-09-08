#pragma once

#include <Camera.h>
#include <Game.h>
#include <IndexBuffer.h>
#include <Light.h>
#include <Window.h>
#include <Mesh.h>
#include <RenderTarget.h>
#include <RootSignature.h>
#include <Texture.h>
#include <VertexBuffer.h>

#include "DirectionalLightShadowPassPso.h"
#include "GameObject.h"
#include "ParticleSystemPso.h"
#include "PointLightPso.h"

class ParticleSystem;

class LightingDemo final : public Game
{
public:
	using Base = Game;

	LightingDemo(const std::wstring& name, int width, int height, bool vSync = false);
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
	std::vector<GameObject> m_GameObjects;
	std::shared_ptr<Texture> m_WhiteTexture2d;

	RenderTarget m_RenderTarget;
	RootSignature m_RootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PipelineState;

	DirectionalLight m_DirectionalLight{};
	std::vector<PointLight> m_PointLights;
	std::unique_ptr<PointLightPso> m_PointLightPso;
	std::unique_ptr<DirectionalLightShadowPassPso> m_DirectionalLightShadowPassPso;

	std::unique_ptr<ParticleSystem> m_ParticleSystem;

	D3D12_VIEWPORT m_Viewport;
	D3D12_RECT m_ScissorRect;

	Camera m_Camera;

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
