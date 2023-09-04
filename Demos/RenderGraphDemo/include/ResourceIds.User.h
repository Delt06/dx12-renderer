#pragma once

#include "ResourceId.h"

namespace ResourceIds
{
    class User
    {
    public:
        static inline const RenderGraph::ResourceId TempRenderTarget = RenderGraph::ResourceIds::GetResourceId(L"TempRenderTarget");
        static inline const RenderGraph::ResourceId TempRenderTarget2 = RenderGraph::ResourceIds::GetResourceId(L"TempRenderTarget2");
    };
}
