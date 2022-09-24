#pragma once

#include <d3d12.h>
#include <wrl.h>
#include "../CommandList.h"

class EffectBase
{
public:
	using IDevice = ID3D12Device2;

	EffectBase() = default;
	virtual ~EffectBase() = default;

	virtual void Init(Microsoft::WRL::ComPtr<IDevice> device, CommandList& commandList) = 0;

	EffectBase(const EffectBase& other) = delete;
	EffectBase(const EffectBase&& other) = delete;

	EffectBase operator=(const EffectBase& other) = delete;
	EffectBase operator=(const EffectBase&& other) = delete;
};