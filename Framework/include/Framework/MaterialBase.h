#pragma once
#include <memory>
#include <DX12Library/Texture.h>
#include <DX12Library/CommandList.h>

class MaterialBase
{
public:
	virtual void SetDynamicConstantBuffer(CommandList& commandList, uint32_t rootParameterIndex) = 0;
	virtual void SetShaderResourceViews(CommandList& commandList, uint32_t rootParameterIndex, uint32_t baseDescriptorOffset = 0) = 0;
	virtual void SetMapsEmpty(std::shared_ptr<Texture> emptyMap) = 0;
};