#pragma once

#include <RenderGraph/ResourceId.h>

namespace ResourceIds
{
    class User
    {
    public:
        static inline const RenderGraph::ResourceId TempRenderTarget = RenderGraph::ResourceIds::GetResourceId(L"TempRenderTarget");
        static inline const RenderGraph::ResourceId TempRenderTarget2 = RenderGraph::ResourceIds::GetResourceId(L"TempRenderTarget2");
        static inline const RenderGraph::ResourceId TempRenderTarget3 = RenderGraph::ResourceIds::GetResourceId(L"TempRenderTarget3");

        static inline const RenderGraph::ResourceId ColorSplitBuffer = RenderGraph::ResourceIds::GetResourceId(L"ColorSplitBuffer");

        static inline const RenderGraph::ResourceId SetupFinishedToken = RenderGraph::ResourceIds::GetResourceId(L"SetupFinishedToken");
        static inline const RenderGraph::ResourceId ColorSplitBufferInitToken = RenderGraph::ResourceIds::GetResourceId(L"ColorSplitBufferInitToken");
        static inline const RenderGraph::ResourceId TempRenderTargetReadyToken = RenderGraph::ResourceIds::GetResourceId(L"TempRenderTargetReadyToken");
    };
}
