#include "StructuredBuffer.h"
#include "d3dx12.h"
#include "Application.h"
#include "Buffer.h"

StructuredBuffer::StructuredBuffer(const std::wstring& name /*= L""*/)
    : Buffer(name)
    , m_CounterBuffer(std::make_shared<ByteAddressBuffer>(CD3DX12_RESOURCE_DESC::Buffer(4, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS), 1, 4, name + L" Counter"))
    , m_NumElements(0)
    , m_ElementSize(0)
{
    m_Srv = Application::Get().AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    m_Uav = Application::Get().AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

StructuredBuffer::StructuredBuffer(const D3D12_RESOURCE_DESC resourceDesc, size_t numElements, size_t elementSize, const std::wstring& name /*= L""*/)
    : Buffer(resourceDesc, numElements, elementSize, name)
    , m_CounterBuffer(std::make_shared<ByteAddressBuffer>(CD3DX12_RESOURCE_DESC::Buffer(4, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS), 1, 4, name + L" Counter"))
    , m_NumElements(numElements)
    , m_ElementSize(elementSize)
{
    m_Srv = Application::Get().AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    m_Uav = Application::Get().AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    CreateViews(m_NumElements, m_ElementSize);
}

StructuredBuffer::StructuredBuffer(const D3D12_RESOURCE_DESC& resourceDesc, const Microsoft::WRL::ComPtr<ID3D12Heap>& pHeap, UINT64 heapOffset, size_t numElements, size_t elementSize, const std::wstring & name)
    : Buffer(resourceDesc, pHeap, heapOffset, numElements, elementSize, name)
    , m_CounterBuffer(std::make_shared<ByteAddressBuffer>(CD3DX12_RESOURCE_DESC::Buffer(4, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS), 1, 4, name + L" Counter"))
    , m_NumElements(numElements)
    , m_ElementSize(elementSize)
{
    // TODO: allocate the counter buffer inside the same heap
    m_Srv = Application::Get().AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    m_Uav = Application::Get().AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    CreateViews(m_NumElements, m_ElementSize);
}

size_t StructuredBuffer::GetNumElements() const
{
    return m_NumElements;
}

size_t StructuredBuffer::GetElementSize() const
{
    return m_ElementSize;
}

void StructuredBuffer::CreateViews(size_t numElements, size_t elementSize)
{
    auto device = Application::Get().GetDevice();

    m_NumElements = numElements;
    m_ElementSize = elementSize;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Buffer.NumElements = static_cast<UINT>(m_NumElements);
    srvDesc.Buffer.StructureByteStride = static_cast<UINT>(m_ElementSize);
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

    device->CreateShaderResourceView(m_d3d12Resource.Get(),
        &srvDesc,
        m_Srv.GetDescriptorHandle());

    if ((GetD3D12ResourceDesc().Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) != 0)
    {
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        uavDesc.Format = DXGI_FORMAT_UNKNOWN;
        uavDesc.Buffer.CounterOffsetInBytes = 0;
        uavDesc.Buffer.NumElements = static_cast<UINT>(m_NumElements);
        uavDesc.Buffer.StructureByteStride = static_cast<UINT>(m_ElementSize);
        uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

        device->CreateUnorderedAccessView(m_d3d12Resource.Get(),
            m_CounterBuffer->GetD3D12Resource().Get(),
            &uavDesc,
            m_Uav.GetDescriptorHandle());
    }
}

D3D12_CPU_DESCRIPTOR_HANDLE StructuredBuffer::GetShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc /*= nullptr*/) const
{
    return m_Srv.GetDescriptorHandle();
}

D3D12_CPU_DESCRIPTOR_HANDLE StructuredBuffer::GetUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* uavDesc /*= nullptr*/) const
{
    return m_Uav.GetDescriptorHandle();
}

ByteAddressBuffer& StructuredBuffer::GetCounterBuffer()
{
    return *m_CounterBuffer;
}

const std::shared_ptr<ByteAddressBuffer>& StructuredBuffer::GetCounterBufferPtr() const
{
    return m_CounterBuffer;
}
