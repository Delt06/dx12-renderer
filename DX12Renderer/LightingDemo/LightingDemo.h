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

#include "GameObject.h"

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
	std::shared_ptr<Texture> m_Texture;

	RenderTarget MRenderTarget;
	RootSignature MRootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> MPipelineState;

	D3D12_VIEWPORT Viewport;
	D3D12_RECT ScissorRect;

	Camera MCamera;

	static constexpr size_t ALIGNMENT = 16;

	struct alignas(ALIGNMENT) CameraData
	{
		DirectX::XMVECTOR InitialPosition;
		DirectX::XMVECTOR InitialQRotation;
	};

	CameraData* PAlignedCameraData;

	struct
	{
		float Forward;
		float Backward;
		float Left;
		float Right;
		float Up;
		float Down;

		float Pitch;
		float Yaw;

		bool Shift;
	} CameraController;

	bool AnimatedLights;

	int Width;
	int Height;

	DirectionalLight MDirectionalLight{};
};
