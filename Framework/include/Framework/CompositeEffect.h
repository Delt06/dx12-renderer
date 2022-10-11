﻿#pragma once

#include <wrl.h>
#include <d3d12.h>
#include <d3dx12.h>
#include <DX12Library/CommandList.h>
#include <string>
#include <DX12Library/RenderTarget.h>
#include <vector>
#include "EffectBase.h"

class CompositeEffect : public EffectBase
{
public:
	using RootParameter = CD3DX12_ROOT_PARAMETER1;
	using DescriptorRange = CD3DX12_DESCRIPTOR_RANGE1;
	using StaticSampler = CD3DX12_STATIC_SAMPLER_DESC;
	using Format = DXGI_FORMAT;
	using ScissorRect = D3D12_RECT;
	using Viewport = CD3DX12_VIEWPORT;
	using BlendMode = CD3DX12_BLEND_DESC;
	using ShaderBytecode = CD3DX12_SHADER_BYTECODE;
	using ShaderBlob = Microsoft::WRL::ComPtr<ID3DBlob>;

	CompositeEffect() = default;
	virtual ~CompositeEffect();

	void Init(Microsoft::WRL::ComPtr<IDevice> device, CommandList& commandList) final;

protected:
	virtual ShaderBytecode GetVertexShaderBytecode() const = 0;
	virtual ShaderBytecode GetPixelShaderBytecode() const = 0;

	virtual std::vector<RootParameter> GetRootParameters() const = 0;
	virtual std::vector<StaticSampler> GetStaticSamplers() const = 0;

	virtual Format GetRenderTargetFormat() const = 0;

	virtual BlendMode GetBlendMode() const;

	virtual void OnPostInit(Microsoft::WRL::ComPtr<IDevice> device, CommandList& commandList) {};

	void SetContext(CommandList& commandList) const;
	static void SetRenderTarget(CommandList& commandList, const RenderTarget& renderTarget, bool autoViewport = true, bool autoScissorRect = true);
	static void SetRenderTarget(CommandList& commandList, const RenderTarget& renderTarget, UINT arrayIndex, bool autoViewport = true, bool autoScissorRect = true);

	[[nodiscard]] static ShaderBlob LoadShaderFromFile(const std::wstring& fileName);

	[[nodiscard]] static ScissorRect GetAutoScissorRect();
	[[nodiscard]] static Viewport GetAutoViewport(const RenderTarget& renderTarget);

	[[nodiscard]] static BlendMode AdditiveBlend();

private:
	static void CombineRootSignatureFlags(D3D12_ROOT_SIGNATURE_FLAGS& flags, const std::vector<RootParameter>& rootParameters);

	static bool CheckRootParametersVisiblity(const std::vector<RootParameter>& rootParameters, D3D12_SHADER_VISIBILITY shaderVisibility);

	RootSignature m_RootSignature{};
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PipelineState = nullptr;
};
