#include "DX12LibPCH.h"

#include "Buffer.h"

Buffer::Buffer(const std::wstring& name)
    : ResourceWrapper(name)
{
}

Buffer::Buffer(const D3D12_RESOURCE_DESC& resDesc,
               const size_t numElements, const size_t elementSize,
               const std::wstring& name)
    : ResourceWrapper(resDesc, nullptr, name)
{
    CreateViews(numElements, elementSize);
}

void Buffer::CreateViews(size_t numElements, size_t elementSize)
{
    throw std::exception("Unimplemented function.");
}
