#include <Framework/Blit.h>
#include <Framework/Mesh.h>
#include <DX12Library/CommandList.h>
#include <DX12Library/Helpers.h>
#include <DX12Library/Texture.h>
#include <DX12Library/RenderTarget.h>
#include <Framework/Blit_PS.h>
#include <Framework/Blit_VS.h>

using namespace Microsoft::WRL;

namespace
{
	namespace RootParameters
	{
		enum RootParameters
		{
			// Texture2D : register(t0)
			Source,
			NumRootParameters
		};
	}
}

[[maybe_unused]] Blit::Blit(Shader::Format renderTargetFormat, bool linearFilter /*= false*/)
	: m_RenderTargetFormat(renderTargetFormat)
	, m_LinearFilter(linearFilter)
{

}

void Blit::Execute(CommandList& commandList, const Texture& source, RenderTarget& destination, UINT destinationTexArrayIndex/*=-1*/)
{
	PIXScope(commandList, "Blit");

	SetContext(commandList);
	SetRenderTarget(commandList, destination, destinationTexArrayIndex);

	commandList.SetShaderResourceView(RootParameters::Source, 0, source, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0, UINT_MAX);

	m_BlitMesh->Draw(commandList);
}

std::vector<Shader::RootParameter> Blit::GetRootParameters() const
{
	DescriptorRange sourceDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	std::vector<RootParameter> rootParameters;
	rootParameters.resize(RootParameters::NumRootParameters);
	rootParameters[RootParameters::Source].InitAsDescriptorTable(1, &sourceDescriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);
	return rootParameters;
}

std::vector<Shader::StaticSampler> Blit::GetStaticSamplers() const
{
	auto filter = m_LinearFilter ? D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR : D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT;
	return { Shader::StaticSampler(0, filter, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP) };
}

Shader::Format Blit::GetRenderTargetFormat() const
{
	return m_RenderTargetFormat;
}

void Blit::OnPostInit(Microsoft::WRL::ComPtr<IDevice> device, CommandList& commandList)
{
	m_BlitMesh = Mesh::CreateBlitTriangle(commandList);
}

Shader::ShaderBytecode Blit::GetVertexShaderBytecode() const
{
	return ShaderBytecode(ShaderBytecode_Blit_VS, sizeof ShaderBytecode_Blit_VS);
}

Shader::ShaderBytecode Blit::GetPixelShaderBytecode() const
{
	return ShaderBytecode(ShaderBytecode_Blit_PS, sizeof ShaderBytecode_Blit_PS);
}
