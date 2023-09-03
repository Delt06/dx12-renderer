#pragma once

#include <memory>

#include "ResourcePool.h"

namespace RenderGraph
{
    struct RenderContext
    {
        std::shared_ptr<ResourcePool> m_ResourcePool = nullptr;
    };
}
