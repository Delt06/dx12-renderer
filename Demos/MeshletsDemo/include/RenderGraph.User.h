#pragma once

#include <memory>

#include <Framework/CommonRootSignature.h>

#include <RenderGraph/RenderGraphRoot.h>

class MeshletsDemo;

namespace RenderGraph
{
    class User
    {
    public:
        static std::unique_ptr<RenderGraphRoot> Create(
            MeshletsDemo& demo,
            const std::shared_ptr<CommonRootSignature>& rootSignature,
            CommandList& commandList
        );
    };
}
