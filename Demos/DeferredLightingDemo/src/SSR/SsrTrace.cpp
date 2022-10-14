#include <SSR/SsrTrace.h>
#include <Framework/Mesh.h>
#include <Framework/Blit_VS.h>


SsrTrace::SsrTrace(const std::shared_ptr<CommonRootSignature>& rootSignature, CommandList& commandList)
	: m_BlitMesh(Mesh::CreateBlitTriangle(commandList))
{
	auto shader = std::make_shared<Shader>(rootSignature,
		ShaderBlob(ShaderBytecode_Blit_VS, sizeof ShaderBytecode_Blit_VS),
		ShaderBlob(L"SSR_Trace_PS.cso")
		);
	m_Material = Material::Create(shader);
}

void SsrTrace::Execute(CommandList& commandList, const std::shared_ptr<Texture>& sceneColor, const RenderTarget& renderTarget) const
{
	const auto colorDesc = sceneColor->GetD3D12ResourceDesc();

	m_Material->SetVariable<uint32_t>("Steps", 5);
	m_Material->SetVariable<float>("Thickness", 0.5f);
	m_Material->SetVariable<DirectX::XMFLOAT2>("TexelSize", {
		1.0f / static_cast<float>(colorDesc.Width),
		1.0f / static_cast<float>(colorDesc.Height),
		}
	);

	m_Material->SetShaderResourceView("sceneColor", ShaderResourceView(sceneColor));

	commandList.SetRenderTarget(renderTarget);
	commandList.SetAutomaticViewportAndScissorRect(renderTarget);

	m_Material->Bind(commandList);
	m_BlitMesh->Draw(commandList);
	m_Material->Unbind(commandList);
}