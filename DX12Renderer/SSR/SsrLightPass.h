#pragma once

#include "Framework/Shader.h"
#include "Mesh.h"

class SsrLightPass final : public Shader
{
public:
	explicit SsrLightPass(Shader::Format renderTargetFormat);

	void Execute(CommandList& commandList, const Texture& traceResult, const RenderTarget& renderTarget) const;

protected:

	std::wstring GetVertexShaderName() const override;

	std::wstring GetPixelShaderName() const override;

	std::vector<RootParameter> GetRootParameters() const override;

	std::vector<StaticSampler> GetStaticSamplers() const override;

	Format GetRenderTargetFormat() const override;

	void OnPostInit(Microsoft::WRL::ComPtr<IDevice> device, CommandList& commandList) override;

	BlendMode GetBlendMode() const override;

private:
	Shader::Format m_RenderTargetFormat;
	std::shared_ptr<Mesh> m_BlitMesh = nullptr;

	std::vector<RootParameter> m_RootParameters;
	DescriptorRange m_SourceDescriptorRange;
};