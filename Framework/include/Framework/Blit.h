#pragma once

#include <d3d12.h>
#include <d3dx12.h>
#include <wrl.h>
#include <DX12Library/RootSignature.h>
#include <memory>
#include "Framework/Shader.h"

class CommandList;
class Mesh;
class Texture;
class RenderTarget;

class Blit final : public Shader
{
public:
	[[maybe_unused]] Blit(Shader::Format renderTargetFormat, bool linearFilter = false);

	void Execute(CommandList& commandList, const Texture& source, RenderTarget& destination, UINT destinationTexArrayIndex = -1);

protected:


	std::vector<RootParameter> GetRootParameters() const override;


	std::vector<StaticSampler> GetStaticSamplers() const override;


	Format GetRenderTargetFormat() const override;


	void OnPostInit(Microsoft::WRL::ComPtr<IDevice> device, CommandList& commandList) override;

	ShaderBytecode GetVertexShaderBytecode() const override;

	ShaderBytecode GetPixelShaderBytecode() const override;

private:
	Shader::Format m_RenderTargetFormat;
	bool m_LinearFilter;
	std::shared_ptr<Mesh> m_BlitMesh = nullptr;
};

