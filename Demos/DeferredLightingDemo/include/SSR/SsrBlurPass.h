#pragma once

#include <Framework/CompositeEffect.h>
#include <Framework/Mesh.h>

class SsrBlurPass final : public CompositeEffect
{
public:
	explicit SsrBlurPass(CompositeEffect::Format renderTargetFormat);

	void Execute(CommandList& commandList, const Texture& traceResult, const RenderTarget& renderTarget) const;

protected:

	std::vector<RootParameter> GetRootParameters() const override;

	std::vector<StaticSampler> GetStaticSamplers() const override;

	Format GetRenderTargetFormat() const override;

	void OnPostInit(Microsoft::WRL::ComPtr<IDevice> device, CommandList& commandList) override;

	ShaderBytecode GetVertexShaderBytecode() const override;

	ShaderBytecode GetPixelShaderBytecode() const override;

private:
	CompositeEffect::Format m_RenderTargetFormat;
	std::shared_ptr<Mesh> m_BlitMesh = nullptr;

	std::vector<RootParameter> m_RootParameters;
	DescriptorRange m_SourceDescriptorRange;

	ShaderBlob m_PixelShader;
};