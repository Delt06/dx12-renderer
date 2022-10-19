#include <d3dcompiler.h>

#include <PBR/BrdfIntegration.h>

#include <Framework/Mesh.h>
#include <DX12Library/Helpers.h>
#include <DX12Library/RenderTarget.h>
#include <DX12Library/ShaderUtils.h>
#include <Framework/Blit_VS.h>

BrdfIntegration::BrdfIntegration(const std::shared_ptr<CommonRootSignature>& rootSignature, CommandList& commandList)
	: m_BlitMesh(Mesh::CreateBlitTriangle(commandList))
{
	auto shader = std::make_shared<Shader>(rootSignature,
		ShaderBlob(ShaderBytecode_Blit_VS, sizeof(ShaderBytecode_Blit_VS)),
		ShaderBlob(L"IBL_BRDFIntegration_PS.cso")
		);
	m_Material = Material::Create(shader);
}

void BrdfIntegration::SetRenderTarget(CommandList& commandList, RenderTarget& renderTarget)
{
	commandList.SetRenderTarget(renderTarget);
	commandList.SetAutomaticViewportAndScissorRect(renderTarget);
}

void BrdfIntegration::Draw(CommandList& commandList)
{
	m_Material->Bind(commandList);
	m_BlitMesh->Draw(commandList);
	m_Material->Unbind(commandList);
}
