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

    struct TextureDescription
    {
        ResourceId m_Id;
        RenderMetadataExpression<uint32_t> m_WidthExpression;
        RenderMetadataExpression<uint32_t> m_HeightExpression;
        DXGI_FORMAT m_Format;
        ClearValue m_ClearValue;

        uint32_t m_ArraySize = 1;
        uint32_t m_MipLevels = 0;
        uint32_t m_SampleCount = 1;

        TextureDescription(const ResourceId id,
            const RenderMetadataExpression<uint32_t>& widthExpression, const RenderMetadataExpression<uint32_t>& heightExpression,
            const DXGI_FORMAT format, const ClearValue::COLOR clearColor
        )
            : m_Id(id)
            , m_WidthExpression(widthExpression)
            , m_HeightExpression(heightExpression)
            , m_Format(format)
            , m_ClearValue(format, clearColor)
        {

        }

        TextureDescription(const ResourceId id,
            const RenderMetadataExpression<uint32_t>& widthExpression, const RenderMetadataExpression<uint32_t>& heightExpression,
            const DXGI_FORMAT format, const ClearValue::DEPTH_STENCIL_VALUE clearDepthStencilValue
        )
            : m_Id(id)
            , m_WidthExpression(widthExpression)
            , m_HeightExpression(heightExpression)
            , m_Format(format)
            , m_ClearValue(format, clearDepthStencilValue)
        {

        }
    };

    struct BufferDescription
    {
        ResourceId m_Id;
    };
}
