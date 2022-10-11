#pragma once

#include <d3d12.h>
#include <DX12Library/Resource.h>

#include <memory>

struct ShaderResourceView
{
	explicit ShaderResourceView(const std::shared_ptr<Resource>& resource,
		UINT firstSubresource = 0, UINT numSubresources = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
		D3D12_SHADER_RESOURCE_VIEW_DESC* desc = nullptr
	)
		: m_Resource(resource)
		, m_FirstSubresource(firstSubresource)
		, m_NumSubresources(numSubresources)
		, m_Desc(desc)
	{

	}

	ShaderResourceView(const ShaderResourceView& other) = default;
	ShaderResourceView& operator=(const ShaderResourceView& other) = default;

	std::shared_ptr<Resource> m_Resource = nullptr;
	UINT m_FirstSubresource;
	UINT m_NumSubresources;
	D3D12_SHADER_RESOURCE_VIEW_DESC* m_Desc;
};