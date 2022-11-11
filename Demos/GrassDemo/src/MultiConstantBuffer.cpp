#include "MultiConstantBuffer.h"

#include <d3dx12.h>
#include <DX12Library/Application.h>
#include <DX12Library/Helpers.h>

MultiConstantBuffer::MultiConstantBuffer(size_t numElements, const std::wstring& name)
    : Buffer(name)
    , m_SizeInBytes(0)
    , m_AlignedItemSize(0)
{
    auto descriptorPage = std::make_shared<DescriptorAllocatorPage>(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, numElements);
    m_ConstantBufferViewBase = descriptorPage->Allocate(numElements);
}

MultiConstantBuffer::~MultiConstantBuffer() = default;

void MultiConstantBuffer::CreateViews(const size_t numElements, const size_t elementSize)
{
    m_AlignedItemSize = static_cast<UINT>(Math::AlignUp(elementSize, 256));
    m_SizeInBytes = numElements * m_AlignedItemSize;

    const auto device = Application::Get().GetDevice();

    for (UINT i = 0; i < numElements; ++i)
    {
        D3D12_CONSTANT_BUFFER_VIEW_DESC bufferViewDesc;
        bufferViewDesc.BufferLocation = GetItemGPUAddress(i);
        bufferViewDesc.SizeInBytes = m_AlignedItemSize;
        device->CreateConstantBufferView(&bufferViewDesc, GetConstantBufferView(i));
    }
}

D3D12_CPU_DESCRIPTOR_HANDLE MultiConstantBuffer::GetShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc) const
{
    throw std::exception("MultiConstantBuffer::GetShaderResourceView should not be called.");
}

D3D12_CPU_DESCRIPTOR_HANDLE MultiConstantBuffer::GetUnorderedAccessView(
    const D3D12_UNORDERED_ACCESS_VIEW_DESC* uavDesc) const
{
    throw std::exception("MultiConstantBuffer::GetUnorderedAccessView should not be called.");
}
