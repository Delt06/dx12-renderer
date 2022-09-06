#pragma once

/*
 *  Copyright(c) 2018 Jeremiah van Oosten
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files(the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions :
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 *  IN THE SOFTWARE.
 */

/**
 *  @file Tutorial3.h
 *  @date October 24, 2018
 *  @author Jeremiah van Oosten
 *
 *  @brief Tutorial 3 class.
 */

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

class Tutorial3 final : public Game
{
public:
	using Base = Game;

	Tutorial3(const std::wstring& name, int width, int height, bool vSync = false);
	~Tutorial3() override;

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
	std::shared_ptr<Mesh> CubeMesh;
	std::shared_ptr<Mesh> SphereMesh;
	std::shared_ptr<Mesh> ConeMesh;
	std::shared_ptr<Mesh> TorusMesh;
	std::shared_ptr<Mesh> PlaneMesh;

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
