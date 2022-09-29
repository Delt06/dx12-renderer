#pragma once

#include <Framework/Shader.h>
#include "BloomParameters.h"

class Mesh;

class BloomUpsample final : public Shader
{
public:
	explicit BloomUpsample(Format backBufferFormat);

	void Begin(CommandList& commandList);

	void Execute(CommandList& commandList,
		const BloomParameters& parameters,
		const Texture& source,
		const RenderTarget& destination);

protected:
	ShaderBytecode GetVertexShaderBytecode() const override;

	ShaderBytecode GetPixelShaderBytecode() const override;

	std::vector<RootParameter> GetRootParameters() const override;

	std::vector<StaticSampler> GetStaticSamplers() const override;

	Format GetRenderTargetFormat() const override;

	BlendMode GetBlendMode() const override;

	void OnPostInit(Microsoft::WRL::ComPtr<IDevice> device, CommandList& commandList) override;

private:
	Format m_BackBufferFormat;
	std::shared_ptr<Mesh> m_BlitMesh = nullptr;

	std::vector<RootParameter> m_RootParameters;
	DescriptorRange m_SourceDescriptorRange;
	std::vector<StaticSampler> m_StaticSamplers;
};
