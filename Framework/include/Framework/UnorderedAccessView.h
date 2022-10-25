#pragma once

#include <d3d12.h>
#include <DX12Library/Resource.h>

#include <memory>

struct UnorderedAccessView
{
    explicit UnorderedAccessView(const std::shared_ptr<Resource>& resource,
        UINT firstSubresource = 0, UINT numSubresources = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES
    )
        : m_Resource(resource)
        , m_FirstSubresource(firstSubresource)
        , m_NumSubresources(numSubresources)
        , m_IsDescValid(false)
        , m_Desc{}
    {

    }

    explicit UnorderedAccessView(const std::shared_ptr<Resource>& resource,
        UINT firstSubresource, UINT numSubresources,
        D3D12_UNORDERED_ACCESS_VIEW_DESC desc
    )
        : m_Resource(resource)
        , m_FirstSubresource(firstSubresource)
        , m_NumSubresources(numSubresources)
        , m_IsDescValid(true)
        , m_Desc(desc)
    {

    }

    UnorderedAccessView(const UnorderedAccessView& other) = default;
    UnorderedAccessView& operator=(const UnorderedAccessView& other) = default;

    const D3D12_UNORDERED_ACCESS_VIEW_DESC* GetDescOrNullptr() const
    {
        if (m_IsDescValid)
            return &m_Desc;
        return nullptr;
    }

    std::shared_ptr<Resource> m_Resource = nullptr;
    UINT m_FirstSubresource;
    UINT m_NumSubresources;

    bool m_IsDescValid;
    D3D12_UNORDERED_ACCESS_VIEW_DESC m_Desc;
};
