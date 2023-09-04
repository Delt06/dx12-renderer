#pragma once

#include "ResourceId.h"

namespace ResourceIds
{
    class User
    {
    public:
        static inline const RenderGraph::ResourceId TempRenderTarget = RenderGraph::ResourceIds::GetResourceId(L"TempRenderTarget");
        static inline const RenderGraph::ResourceId TempRenderTarget2 = RenderGraph::ResourceIds::GetResourceId(L"TempRenderTarget2");
        static inline const RenderGraph::ResourceId TempRenderTarget3 = RenderGraph::ResourceIds::GetResourceId(L"TempRenderTarget3");
        static inline const RenderGraph::ResourceId TempRenderTargetReadyToken = RenderGraph::ResourceIds::GetResourceId(L"TempRenderTargetReadyToken");
    };
}
