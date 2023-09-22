#pragma once

#include <memory>

#include <DX12Library/RenderTarget.h>

namespace RenderGraph
{
    struct RenderTargetInfo
    {
        std::shared_ptr<RenderTarget> m_RenderTarget = nullptr;
        bool m_ReadonlyDepth = false;
    };
}
