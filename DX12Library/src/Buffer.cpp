#include "DX12LibPCH.h"
#include "Application.h"

#include "Buffer.h"

Buffer::Buffer(const std::wstring& name)
    : Resource(name)
{}

Buffer::Buffer(const D3D12_RESOURCE_DESC& resDesc,
    const size_t numElements, const size_t elementSize,
    const std::wstring& name)
    : Resource(resDesc, nullptr, name)
{}

Buffer::Buffer(const D3D12_RESOURCE_DESC & resDesc, const Microsoft::WRL::ComPtr<ID3D12Heap>& pHeap, UINT64 heapOffset, size_t numElements, size_t elementSize, const std::wstring & name)
    : Resource(resDesc, pHeap, heapOffset, nullptr, name)
{

}

const Microsoft::WRL::ComPtr<ID3D12Resource>& Buffer::GetUploadResource(size_t size) const
{
    if (m_UploadResource != nullptr && m_UploadResourceDesc.Width >= size)
    {
        return m_UploadResource;
    }

    m_UploadResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(size);

    const auto device = Application::Get().GetDevice();
    const auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    ThrowIfFailed(device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &m_UploadResourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_UploadResource)));
    return m_UploadResource;
}
