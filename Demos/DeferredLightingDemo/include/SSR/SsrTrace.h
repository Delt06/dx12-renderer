#pragma once
#include <Framework/Shader.h>
#include <DirectXMath.h>

class Mesh;

class SsrTrace final : public Shader
{
public: 
	explicit SsrTrace(Format renderTargetFormat);

	void SetDepthSrv(const D3D12_SHADER_RESOURCE_VIEW_DESC* depthSrv);
	void SetJitterOffset(DirectX::XMFLOAT2 jitterOffset);

	void Execute(CommandList& commandList, const Texture& sceneColor, const Texture& normals, const Texture& surface, const Texture& depth, const RenderTarget& renderTarget, DirectX::XMMATRIX viewMatrix, DirectX::XMMATRIX projectionMatrix) const;

protected:
	std::vector<RootParameter> GetRootParameters() const override;

	std::vector<StaticSampler> GetStaticSamplers() const override;

	Format GetRenderTargetFormat() const override;

	void OnPostInit(Microsoft::WRL::ComPtr<IDevice> device, CommandList& commandList) override;

	ShaderBytecode GetVertexShaderBytecode() const override;

	ShaderBytecode GetPixelShaderBytecode() const override;


private:
	std::shared_ptr<Mesh> m_BlitMesh;
	Format m_RenderTargetFormat;
	const D3D12_SHADER_RESOURCE_VIEW_DESC* m_PDepthSrv = nullptr;
	DirectX::XMFLOAT2 m_JitterOffset{};

	std::vector<RootParameter> m_RootParameters;
	DescriptorRange m_SourceDescriptorRange;

	ShaderBlob m_PixelShader;
};

