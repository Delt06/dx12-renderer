#pragma once
#include <Framework/CompositeEffect.h>
#include <Framework/Mesh.h>
#include <memory>
#include <Framework/MatricesCb.h>

class Reflections final : public CompositeEffect
{
public:
	explicit Reflections(CompositeEffect::Format renderTargetFormat);

	void SetSrvDescriptors(const D3D12_SHADER_RESOURCE_VIEW_DESC& preFilterSrvDesc, const D3D12_SHADER_RESOURCE_VIEW_DESC& depthSrvDesc);
	void Draw(CommandList& commandList, const RenderTarget& gBufferRenderTarget, const Texture& depthTexture, const Texture& preFilterMap, const Texture& brdfLut, const Texture& ssrTexture, const MatricesCb& matrices);

protected:

	std::vector<RootParameter> GetRootParameters() const override;

	std::vector<StaticSampler> GetStaticSamplers() const override;

	Format GetRenderTargetFormat() const override;

	BlendMode GetBlendMode() const override;

	void OnPostInit(Microsoft::WRL::ComPtr<IDevice> device, CommandList& commandList) override;

	ShaderBytecode GetVertexShaderBytecode() const override;

	ShaderBytecode GetPixelShaderBytecode() const override;


private:
	CompositeEffect::Format m_RenderTargetFormat;
	std::shared_ptr<Mesh> m_BlitMesh;

	std::vector<CompositeEffect::RootParameter> m_RootParameters;
	CompositeEffect::DescriptorRange m_GBufferDescriptorRange;
	CompositeEffect::DescriptorRange m_ReflectionsDescriptorRange;

	D3D12_SHADER_RESOURCE_VIEW_DESC m_PreFilterSrvDesc;
	D3D12_SHADER_RESOURCE_VIEW_DESC m_DepthSrvDesc;

	ShaderBlob m_PixelShader;
};

