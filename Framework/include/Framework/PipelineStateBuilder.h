#pragma once

#include <DX12Library/RootSignature.h>

#include <wrl.h>
#include <d3d12.h>
#include <d3dx12.h>

#include <memory>
#include <vector>

class PipelineStateBuilder final
{
public:
	explicit PipelineStateBuilder(const std::shared_ptr<RootSignature> rootSignature);

	Microsoft::WRL::ComPtr<ID3D12PipelineState> Build(Microsoft::WRL::ComPtr<ID3D12Device2> device) const;

	PipelineStateBuilder& WithRenderTargetFormats(const std::vector<DXGI_FORMAT>& renderTargetFormats, DXGI_FORMAT depthStencilFormat);
	PipelineStateBuilder& WithShaders(const Microsoft::WRL::ComPtr<ID3DBlob>& vertexShader, const Microsoft::WRL::ComPtr<ID3DBlob>& pixelShader);
	PipelineStateBuilder& WithBlend(const CD3DX12_BLEND_DESC& blendDesc);
	PipelineStateBuilder& WithDepthStencil(const CD3DX12_DEPTH_STENCIL_DESC1& depthStencil);
	PipelineStateBuilder& WithInputLayout(const std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout);

private:
	const std::shared_ptr<RootSignature> m_RootSignature;

	std::vector<DXGI_FORMAT> m_RenderTargetFormats;
	DXGI_FORMAT m_DepthStencilFormat;

	Microsoft::WRL::ComPtr<ID3DBlob> m_VertexShader;
	Microsoft::WRL::ComPtr<ID3DBlob> m_PixelShader;
	CD3DX12_BLEND_DESC m_BlendDesc = CD3DX12_BLEND_DESC(CD3DX12_DEFAULT());
	CD3DX12_DEPTH_STENCIL_DESC1 m_DepthStencilDesc = CD3DX12_DEPTH_STENCIL_DESC1(CD3DX12_DEFAULT());
	std::vector<D3D12_INPUT_ELEMENT_DESC> m_InputLayout;
};