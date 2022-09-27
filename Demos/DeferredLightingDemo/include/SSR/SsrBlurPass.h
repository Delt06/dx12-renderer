#pragma once

#include <Framework/Shader.h>
#include <Framework/Mesh.h>

class SsrBlurPass final : public Shader
{
public:
	explicit SsrBlurPass(Shader::Format renderTargetFormat);

	void Execute(CommandList& commandList, const Texture& traceResult, const RenderTarget& renderTarget) const;

protected:

	std::wstring GetVertexShaderName() const override;

	std::wstring GetPixelShaderName() const override;

	std::vector<RootParameter> GetRootParameters() const override;

	std::vector<StaticSampler> GetStaticSamplers() const override;

	Format GetRenderTargetFormat() const override;

	void OnPostInit(Microsoft::WRL::ComPtr<IDevice> device, CommandList& commandList) override;

private:
	Shader::Format m_RenderTargetFormat;
	std::shared_ptr<Mesh> m_BlitMesh = nullptr;

	std::vector<RootParameter> m_RootParameters;
	DescriptorRange m_SourceDescriptorRange;
};