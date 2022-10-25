#include <HDR/ToneMapping.h>
#include <DX12Library/ShaderUtils.h>
#include <DX12Library/Helpers.h>
#include <DX12Library/RenderTarget.h>
#include <Framework/Mesh.h>
#include <Framework/Blit_VS.h>

using namespace Microsoft::WRL;

namespace
{
	struct Parameters
	{
		float WhitePoint;
		float _Padding[3];
	};
}

ToneMapping::ToneMapping(const std::shared_ptr<CommonRootSignature>& rootSignature, CommandList& commandList)
	: m_BlitMesh(Mesh::CreateBlitTriangle(commandList))
{
	auto shader = std::make_shared<Shader>(rootSignature,
		ShaderBlob(ShaderBytecode_Blit_VS, sizeof ShaderBytecode_Blit_VS),
		ShaderBlob(L"HDR_ToneMapping_PS.cso")
		);
	m_Material = Material::Create(shader);
}

void ToneMapping::Blit(CommandList& commandList, const std::shared_ptr<Texture>& source, const std::shared_ptr<Texture>& luminanceOutput, RenderTarget& destination, float whitePoint /*= 4.0f*/)
{
	PIXScope(commandList, "Tone Mapping");

	m_Material->SetShaderResourceView("source", ShaderResourceView(source));
	m_Material->SetShaderResourceView("averageLuminanceMap", ShaderResourceView(luminanceOutput));

	Parameters parameters{};
	parameters.WhitePoint = whitePoint;
	m_Material->SetAllVariables(parameters);

	commandList.SetRenderTarget(destination);
	commandList.SetAutomaticViewportAndScissorRect(destination);

	m_Material->Bind(commandList);
	m_BlitMesh->Draw(commandList);
	m_Material->Unbind(commandList);
}