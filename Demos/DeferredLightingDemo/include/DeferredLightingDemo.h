#pragma once

#include <DX12Library/Camera.h>
#include <DX12Library/Game.h>
#include <DX12Library/IndexBuffer.h>
#include <DX12Library/Window.h>
#include <DX12Library/RenderTarget.h>
#include <DX12Library/RootSignature.h>
#include <DX12Library/Texture.h>
#include <DX12Library/VertexBuffer.h>

#include <Framework/Mesh.h>
#include <Framework/Light.h>

#include <Framework/GameObject.h>
#include <Framework/GraphicsSettings.h>
#include <HDR/ToneMapping.h>
#include <Ssao.h>
#include <Framework/TAA.h>
#include <SSR/Ssr.h>
#include <Reflections.h>
#include "Framework/Bloom.h"
#include "Framework/CommonRootSignature.h"

struct MatricesCb;
class AutoExposure;

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

	enum class GBufferTextureType
	{
		Diffuse,
		Normals,
		Surface,
		Velocity,
		DepthStencil
	};

	static AttachmentPoint GetGBufferTextureAttachmentPoint(GBufferTextureType type);
	const std::shared_ptr<Texture>& GetGBufferTexture(GBufferTextureType type);

	void LightStencilPass(CommandList& commandList,
		const DirectX::XMMATRIX& lightWorldMatrix,
		const DirectX::XMMATRIX& viewProjectionMatrix,
		const std::shared_ptr<Mesh>& mesh);

	D3D12_SHADER_RESOURCE_VIEW_DESC GetDepthTextureSrv() const;

	void PointLightPass(CommandList& commandList, const PointLight& pointLight, const std::shared_ptr<Mesh>& mesh);
	void SpotLightPass(CommandList& commandList, const SpotLight& spotLight, const std::shared_ptr<Mesh>& mesh);
	void CapsuleLightPass(CommandList& commandList, const CapsuleLight& capsuleLight, const std::shared_ptr<Mesh>& mesh);

	std::shared_ptr<Texture> m_WhiteTexture2d;

	RenderTarget m_GBufferRenderTarget;
	RenderTarget m_LightBufferRenderTarget;
	RenderTarget m_LightStencilRenderTarget;
	RenderTarget m_ResultRenderTarget;
	std::shared_ptr<Texture> m_DepthTexture;

	bool m_SsaoEnabled = true;
	std::unique_ptr<Ssao> m_Ssao;
	RenderTarget m_SurfaceRenderTarget;

	bool m_SsrEnabled = true;
	std::unique_ptr<Ssr> m_Ssr;
	std::unique_ptr<Reflections> m_ReflectionsPass;

	bool m_BloomEnabled = true;
	std::unique_ptr<Bloom> m_Bloom;

	std::unique_ptr<AutoExposure> m_AutoExposurePso;
	std::unique_ptr<ToneMapping> m_ToneMappingPso;

	Camera m_Camera;
	std::vector<GameObject> m_GameObjects;
	bool m_AnimateLights = false;
	float m_DeltaTime;

	bool m_TaaEnabled = true;
	std::unique_ptr<TAA> m_Taa;

	std::shared_ptr<CommonRootSignature> m_CommonRootSignature = nullptr;

	DirectionalLight m_DirectionalLight;
	std::vector<PointLight> m_PointLights;
	std::vector<SpotLight> m_SpotLights;
	std::vector<CapsuleLight> m_CapsuleLights;

	std::shared_ptr<Material> m_DirectionalLightPassMaterial;
	std::shared_ptr<Mesh> m_FullScreenMesh;

	std::shared_ptr<Material> m_LightStencilPasssMaterial;

	std::shared_ptr<Material> m_PointLightPassMaterial;
	std::shared_ptr<Mesh> m_PointLightMesh;

	std::shared_ptr<Material> m_SpotLightPassMaterial;
	std::shared_ptr<Mesh> m_SpotLightMesh;

	std::shared_ptr<Material> m_CapsuleLightPassMaterial;
	std::shared_ptr<Mesh> m_CapsuleLightMesh;

	std::shared_ptr<Material> m_SkyboxPassMaterial;
	std::shared_ptr<Texture> m_Skybox;
	std::shared_ptr<Mesh> m_SkyboxMesh;

	RenderTarget m_DiffuseIrradianceMapRt;
	RenderTarget m_BrdfIntegrationMapRt;
	RenderTarget m_PreFilterEnvironmentMapRt;

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
