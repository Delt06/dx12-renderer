#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace RenderGraph
{
    typedef uint32_t ResourceId;

    class ResourceIds
    {
    public:
        static ResourceId GetResourceId(const wchar_t* name);
        static const std::wstring& GetResourceName(ResourceId id);

        static const ResourceId GraphOutput;

    private:
        static inline std::vector<std::wstring> s_Names = { L"Null" };
        static inline uint32_t s_Count = 1;
        static inline std::map<std::wstring, RenderGraph::ResourceId> s_ExistingIds = {};
    };
}
