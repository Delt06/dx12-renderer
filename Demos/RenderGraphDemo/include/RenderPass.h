#pragma once

#include <memory>

#include <DX12Library/CommandList.h>

class RenderPass
{
public:
    virtual void Execute(CommandList& commandList) = 0;
};
