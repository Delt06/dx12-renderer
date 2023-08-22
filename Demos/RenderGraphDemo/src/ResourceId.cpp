#include "ResourceId.h"

#include <DX12Library/Helpers.h>

const RenderGraph::ResourceId RenderGraph::ResourceIds::GraphOutput = RenderGraph::ResourceIds::GetResourceId(L"BuiltIn-GraphOutput");

RenderGraph::ResourceId RenderGraph::ResourceIds::GetResourceId(const wchar_t* name)
{
    std::wstring nameString = { name };
    const auto findResult = s_ExistingIds.find(nameString);
    if (findResult != s_ExistingIds.end())
    {
        return findResult->second;
    }

    RenderGraph::ResourceId id = s_Count++;
    s_Names.push_back(nameString);
    s_ExistingIds.insert(std::pair<std::wstring, RenderGraph::ResourceId> {nameString, id});
    return id;
}

const std::wstring& RenderGraph::ResourceIds::GetResourceName(RenderGraph::ResourceId id)
{
    Assert(id < s_Names.size(), "ID is invalid.");
    return s_Names[id];
}
