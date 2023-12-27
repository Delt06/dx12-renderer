#include "ResourceId.h"

#include <DX12Library/Helpers.h>

using namespace RenderGraph;

const ResourceId ResourceIds::GRAPH_OUTPUT = GetResourceId(L"RenderGraph-BuiltIn-GraphOutput");

ResourceId ResourceIds::GetResourceId(const wchar_t* name)
{
    std::wstring nameString = { name };
    if (const auto findResult = s_ExistingIds.find(nameString); findResult != s_ExistingIds.end())
    {
        return findResult->second;
    }

    ResourceId id = s_Count++;
    s_Names.push_back(nameString);
    s_ExistingIds.insert(std::pair{ nameString, id });
    return id;
}

const std::wstring& ResourceIds::GetResourceName(ResourceId id)
{
    Assert(id < s_Names.size(), "ID is invalid.");
    return s_Names[id];
}
