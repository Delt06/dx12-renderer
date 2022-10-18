#include <Framework/BloomUpsample.h>
#include <Framework/Blit_VS.h>
#include <Framework/Bloom_Downsample_PS.h>
#include <Framework/Mesh.h>
#include <DirectXMath.h>
#include <DX12Library/Helpers.h>

using namespace DirectX;

namespace
{
	namespace RootParameters
	{
		enum
		{
			SourceTexture = 0,
			Parameters,
			NumRootParameters
		};

		struct ParametersCb
		{
			XMFLOAT2 TexelSize;
			float Intensity;
			float Padding;
		};
	}
}

BloomUpsample::BloomUpsample(const std::shared_ptr<CommonRootSignature>& rootSignature, CommandList& commandList)
	: m_BlitMesh(Mesh::CreateBlitTriangle(commandList))
{
	auto shader = std::make_shared<Shader>(rootSignature,
		ShaderBlob(ShaderBytecode_Blit_VS, sizeof ShaderBytecode_Blit_VS),
		ShaderBlob(ShaderBytecode_Bloom_Downsample_PS, sizeof ShaderBytecode_Bloom_Downsample_PS),
		[](PipelineStateBuilder& builder)
		{
			builder.WithAdditiveBlend();
		}
	);
	m_Material = Material::Create(shader);
}

void BloomUpsample::Execute(CommandList& commandList, const BloomParameters& parameters, const std::shared_ptr<Texture>& source, const RenderTarget& destination)
{
	PIXScope(commandList, "Bloom Upsample");

	commandList.SetRenderTarget(destination);
	commandList.SetAutomaticViewportAndScissorRect(destination);

	m_Material->SetShaderResourceView("sourceColorTexture", ShaderResourceView(source));

	RootParameters::ParametersCb parametersCb{};
	parametersCb.Intensity = parameters.Intensity;
	auto sourceDesc = source->GetD3D12ResourceDesc();
	auto fSourceWidth = static_cast<float>(sourceDesc.Width);
	auto fSourceHeight = static_cast<float>(sourceDesc.Height);
	parametersCb.TexelSize = { 0.5f / fSourceWidth , 0.5f / fSourceHeight }; // 0.5 is for more focused blur
	m_Material->SetAllVariables(parametersCb);

	m_Material->Bind(commandList);
	m_BlitMesh->Draw(commandList);
	m_Material->Unbind(commandList);
}