#pragma once

#include "cstdint"
#include <functional>

#include "dxgi.h"
#include <d3d12.h>

#include "DX12Library/ClearValue.h"

#include "RenderMetadata.h"
#include "ResourceId.h"

namespace RenderGraph
{
    template<typename T>
    using RenderMetadataExpression = std::function<T(const RenderMetadata&)>;

    enum ResourceInitAction
    {
        Clear,
        Discard,
        CopyDestination,
    };

    struct TextureDescription
    {
        ResourceId m_Id;
        RenderMetadataExpression<uint32_t> m_WidthExpression;
        RenderMetadataExpression<uint32_t> m_HeightExpression;
        DXGI_FORMAT m_Format;
        ClearValue m_ClearValue;
        ResourceInitAction m_InitAction;

        uint32_t m_ArraySize = 1;
        uint32_t m_MipLevels = 1;
        uint32_t m_SampleCount = 1;

        TextureDescription()
            : m_Id(0)
            , m_WidthExpression(nullptr)
            , m_HeightExpression(nullptr)
            , m_Format(DXGI_FORMAT_UNKNOWN)
            , m_ClearValue()
            , m_InitAction(ResourceInitAction::Clear)
        {

        }

        TextureDescription(const ResourceId id,
            const RenderMetadataExpression<uint32_t>& widthExpression, const RenderMetadataExpression<uint32_t>& heightExpression,
            const DXGI_FORMAT format, const ClearValue::COLOR clearColor, ResourceInitAction initAction
        )
            : m_Id(id)
            , m_WidthExpression(widthExpression)
            , m_HeightExpression(heightExpression)
            , m_Format(format)
            , m_ClearValue(format, clearColor)
            , m_InitAction(initAction)
        {

        }

        TextureDescription(const ResourceId id,
            const RenderMetadataExpression<uint32_t>& widthExpression, const RenderMetadataExpression<uint32_t>& heightExpression,
            const DXGI_FORMAT format, const ClearValue::DEPTH_STENCIL_VALUE clearDepthStencilValue, ResourceInitAction initAction
        )
            : m_Id(id)
            , m_WidthExpression(widthExpression)
            , m_HeightExpression(heightExpression)
            , m_Format(format)
            , m_ClearValue(format, clearDepthStencilValue)
            , m_InitAction(initAction)
        {

        }
    };

    struct BufferDescription
    {
        ResourceId m_Id;
        RenderMetadataExpression<size_t> m_SizeExpression;
        size_t m_Stride;
        ResourceInitAction m_InitAction;

        BufferDescription()
            : m_Id(0)
            , m_SizeExpression(nullptr)
            , m_Stride(0)
            , m_InitAction(ResourceInitAction::Clear)
        {

        }

        BufferDescription(const ResourceId id, const RenderMetadataExpression<size_t>& sizeExpression, const size_t stride, const ResourceInitAction initAction)
            : m_Id(id)
            , m_SizeExpression(sizeExpression)
            , m_Stride(stride)
            , m_InitAction(initAction)
        {

        }
    };

    struct TokenDescription
    {
        ResourceId m_Id;
    };

    enum class ResourceType
    {
        Texture,
        Buffer,
        Token,
    };

    struct ResourceDescription
    {
        ResourceId m_Id;
        D3D12_RESOURCE_DESC m_DxDesc;
        uint64_t m_TotalSize;
        uint64_t m_ElementsCount;
        uint64_t m_Alignment;
        ResourceType m_ResourceType;

        TextureDescription m_TextureDescription;
        TextureUsageType m_TextureUsageType;

        BufferDescription m_BufferDescription;

        TokenDescription m_TokenDescription;

        ResourceInitAction GetInitAction() const
        {
            switch (m_ResourceType)
            {
            case ResourceType::Texture:
                return m_TextureDescription.m_InitAction;
            case ResourceType::Buffer:
                return m_BufferDescription.m_InitAction;
            default:
                Assert(false);
                return ResourceInitAction::Clear;
            }
        }

        const ClearValue& GetClearValue() const
        {
            switch (m_ResourceType)
            {
            case ResourceType::Texture:
                return m_TextureDescription.m_ClearValue;
            default:
                Assert(false);
                return m_TextureDescription.m_ClearValue;
            }
        }
    };
}
