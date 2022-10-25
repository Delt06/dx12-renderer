#include "Shader.h"
#include <DX12Library/ShaderUtils.h>
#include <DX12Library/Application.h>

Shader::Shader(const std::shared_ptr<CommonRootSignature>& rootSignature, const ShaderBlob& vertexShader, const ShaderBlob& pixelShader, const std::function<void(PipelineStateBuilder&)> buildPipelineState)
	: m_RootSignature(rootSignature)
	, m_PipelineStateBuilder(rootSignature)
{
	m_PipelineStateBuilder.WithShaders(vertexShader.GetBlob(), pixelShader.GetBlob());
	buildPipelineState(m_PipelineStateBuilder);

	CollectShaderMetadata(vertexShader.GetBlob(), &m_VertexShaderMetadata);
	CollectShaderMetadata(pixelShader.GetBlob(), &m_PixelShaderMetadata);
}

void Shader::Bind(CommandList& commandList)
{
	const auto device = Application::Get().GetDevice();
	const auto& renderTargetFormats = commandList.GetLastRenderTargetFormats();
	const auto pipelineState = GetPipelineState(device, renderTargetFormats);

	commandList.SetPipelineState(pipelineState);
}

void Shader::Unbind(CommandList& commandList)
{
	m_RootSignature->UnbindMaterialShaderResourceViews(commandList);
}

void Shader::SetMaterialConstantBuffer(CommandList& commandList, size_t size, const void* data)
{
	m_RootSignature->SetMaterialConstantBuffer(commandList, size, data);
}

void Shader::SetShaderResourceView(CommandList& commandList, const std::string& variableName, const ShaderResourceView& shaderResourceView)
{
	const auto vsFindResult = m_VertexShaderMetadata.m_ShaderResourceViewsNameCache.find(variableName);
	const auto vsFound = vsFindResult != m_VertexShaderMetadata.m_ShaderResourceViewsNameCache.end();
	if (vsFound)
	{
		auto index = vsFindResult->second;
		const auto& srvMetadata = m_VertexShaderMetadata.m_ShaderResourceViews[index];
		switch (srvMetadata.Space)
		{
		case CommonRootSignature::MATERIAL_REGISTER_SPACE:
			m_RootSignature->SetMaterialShaderResourceView(commandList, srvMetadata.RegisterIndex, shaderResourceView);
			break;
		case CommonRootSignature::PIPELINE_REGISTER_SPACE:
			m_RootSignature->SetPipelineShaderResourceView(commandList, srvMetadata.RegisterIndex, shaderResourceView);
			break;
		default:
			throw std::exception("Invalid space index for an SRV.");
		}
	}

	const auto psFindResult = m_PixelShaderMetadata.m_ShaderResourceViewsNameCache.find(variableName);
	const auto psFound = psFindResult != m_PixelShaderMetadata.m_ShaderResourceViewsNameCache.end();
	if (psFound)
	{
		auto index = psFindResult->second;
		const auto& srvMetadata = m_PixelShaderMetadata.m_ShaderResourceViews[index];
		switch (srvMetadata.Space)
		{
		case CommonRootSignature::MATERIAL_REGISTER_SPACE:
			m_RootSignature->SetMaterialShaderResourceView(commandList, srvMetadata.RegisterIndex, shaderResourceView);
			break;
		case CommonRootSignature::PIPELINE_REGISTER_SPACE:
			m_RootSignature->SetPipelineShaderResourceView(commandList, srvMetadata.RegisterIndex, shaderResourceView);
			break;
		default:
			throw std::exception("Invalid space index for an SRV.");
		}
	}

	if (!vsFound && !psFound)
	{
		throw std::exception("Shader variable not found.");
	}
}

Microsoft::WRL::ComPtr<ID3D12PipelineState> Shader::GetPipelineState(const Microsoft::WRL::ComPtr<ID3D12Device2>& device, const RenderTargetFormats& formats)
{
	auto findResult = m_PipelineStateObjects.find(formats);
	if (findResult == m_PipelineStateObjects.end())
	{
		std::vector<DXGI_FORMAT> renderTargetFormats(formats.GetCount());
		memcpy(renderTargetFormats.data(), formats.GetFormats(), sizeof(DXGI_FORMAT) * renderTargetFormats.size());
		m_PipelineStateBuilder.WithRenderTargetFormats(renderTargetFormats, formats.GetDepthStencilFormat());
		const auto pipelineStateObject = m_PipelineStateBuilder.Build(device);

		m_PipelineStateObjects.insert(std::make_pair(formats, pipelineStateObject));

		return pipelineStateObject;
	}
	else
	{
		return findResult->second;
	}
}

void Shader::CollectShaderMetadata(const Microsoft::WRL::ComPtr<ID3DBlob>& shader, ShaderMetadata* outMetadata)
{
	const auto reflection = ShaderUtils::Reflect(shader);

	// constant buffers
	{
		outMetadata->m_ConstantBuffers = std::move(ShaderUtils::GetConstantBuffers(reflection));

		for (size_t i = 0; i < outMetadata->m_ConstantBuffers.size(); ++i)
		{
			const auto& cbufferMetadata = outMetadata->m_ConstantBuffers[i];
			outMetadata->m_ConstantBuffersNameCache.emplace(cbufferMetadata.Name, i);
		}
	}

	// shader resource views
	{
		outMetadata->m_ShaderResourceViews = std::move(ShaderUtils::GetShaderResourceViews(reflection));

		for (size_t i = 0; i < outMetadata->m_ShaderResourceViews.size(); ++i)
		{
			const auto& srvMetadata = outMetadata->m_ShaderResourceViews[i];
			outMetadata->m_ShaderResourceViewsNameCache.emplace(srvMetadata.Name, i);
		}
	}
}
