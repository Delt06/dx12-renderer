#include <SSR/SsrBlurPass.h>
#include <Framework/Mesh.h>
#include <Framework/Blit_VS.h>

SsrBlurPass::SsrBlurPass(const std::shared_ptr<CommonRootSignature>& rootSignature, CommandList& commandList)
	: m_BlitMesh(Mesh::CreateBlitTriangle(commandList))
{
	auto shader = std::make_shared<Shader>(rootSignature,
		ShaderBlob(ShaderBytecode_Blit_VS, sizeof ShaderBytecode_Blit_VS),
		ShaderBlob(L"SSR_BlurPass_PS.cso")
		);
	m_Material = Material::Create(shader);
}

void SsrBlurPass::Execute(CommandList& commandList, const std::shared_ptr<Texture>& traceResult, const RenderTarget& renderTarget) const
{
	const auto traceResultDesc = traceResult->GetD3D12ResourceDesc();

	m_Material->SetVariable<DirectX::XMFLOAT2>("TexelSize",
		{
		1.0f / static_cast<float>(traceResultDesc.Width),
		1.0f / static_cast<float>(traceResultDesc.Height)
		}
	);
	m_Material->SetVariable<int32_t>("Size", 1);
	m_Material->SetVariable<float>("Separation", 0.5f);
	m_Material->SetShaderResourceView("traceResult", ShaderResourceView(traceResult));

	commandList.SetRenderTarget(renderTarget);
	commandList.SetAutomaticViewportAndScissorRect(renderTarget);

	m_Material->Bind(commandList);
	m_BlitMesh->Draw(commandList);
	m_Material->Unbind(commandList);
}