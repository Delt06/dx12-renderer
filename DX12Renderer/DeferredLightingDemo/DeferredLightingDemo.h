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
#include "GraphicsSettings.h"

struct MatricesCb;

class DeferredLightingDemo final : public Game
{
public:
	using Base = Game;

	DeferredLightingDemo(const std::wstring& name, int width, int height, GraphicsSettings graphicsSettings);
	~DeferredLightingDemo() override;

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

	struct ScreenParameters
	{
		float Width, Height;
		float OneOverWidth, OneOverHeight;
	};

	void LightStencilPass(CommandList& commandList, const MatricesCb& matricesCb, std::shared_ptr<Mesh> mesh);

	void BindGBufferAsSRV(CommandList& commandList, uint32_t rootParameterIndex);
	void PointLightPass(CommandList& commandList, const MatricesCb& matricesCb, const PointLight& pointLight, const ScreenParameters& screenParameters, std::shared_ptr<Mesh> mesh);
	void SpotLightPass(CommandList& commandList, const MatricesCb& matricesCb, const SpotLight& spotLight, const ScreenParameters& screenParameters, const std::shared_ptr<Mesh> mesh);
	void CapsuleLightPass(CommandList& commandList, const MatricesCb& matricesCb, const CapsuleLight& capsuleLight, const ScreenParameters& screenParameters, const std::shared_ptr<Mesh> mesh);

	std::shared_ptr<Texture> m_WhiteTexture2d;

	RenderTarget m_GBufferRenderTarget;
	RenderTarget m_LightBufferRenderTarget;
	RenderTarget m_LightStencilRenderTarget;
	Texture m_DepthBuffer;
	Texture m_DepthTexture;

	Camera m_Camera;
	std::vector<GameObject> m_GameObjects;
	bool m_AnimateLights = false;

	RootSignature m_GBufferPassRootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_GBufferPassPipelineState;

	DirectionalLight m_DirectionalLight;
	std::vector<PointLight> m_PointLights;
	std::vector<SpotLight> m_SpotLights;
	std::vector<CapsuleLight> m_CapsuleLights;

	RootSignature m_DirectionalLightPassRootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_DirectionalLightPassPipelineState;
	std::shared_ptr<Mesh> m_FullScreenMesh;

	RootSignature m_LightStencilPassRootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_LightStencilPassPipelineState;

	RootSignature m_PointLightPassRootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PointLightPassPipelineState;
	std::shared_ptr<Mesh> m_PointLightMesh;

	RootSignature m_SpotLightPassRootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_SpotLightPassPipelineState;
	std::shared_ptr<Mesh> m_SpotLightMesh;

	RootSignature m_CapsuleLightPassRootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_CapsuleLightPassPipelineState;
	std::shared_ptr<Mesh> m_CapsuleLightMesh;

	D3D12_VIEWPORT m_Viewport;
	D3D12_RECT m_ScissorRect;

	GraphicsSettings m_GraphicsSettings;

	double m_Time = 0;

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
