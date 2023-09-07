#pragma once

#include <memory>

#include "RenderMetadata.h"
#include "ResourcePool.h"

namespace RenderGraph
{
    struct RenderContext
    {
        std::shared_ptr<ResourcePool> m_ResourcePool = nullptr;
        RenderMetadata m_Metadata = {};
    };
}
