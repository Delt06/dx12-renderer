#include <Framework/SharedUploadBuffer.h>

#include <d3dx12.h>

#include <DX12Library/Helpers.h>

void SharedUploadBuffer::BeginFrame()
{
    const auto frameCount = Application::GetFrameCount();
    auto& bufferInfo = GetBufferInfoForFrame(frameCount);
    if (bufferInfo.m_Buffer != nullptr)
    {
        bufferInfo.m_DataCur = bufferInfo.m_DataBegin;
    }
}

void SharedUploadBuffer::Upload(CommandList& commandList, const Resource& destination, const void* pData, const uint64_t sizeInBytes, const uint64_t alignment, const uint64_t destinationOffset)
{
    const auto frameCount = Application::GetFrameCount();
    auto& bufferInfo = GetBufferInfoForFrame(frameCount);
    uint8_t* pUploadPtr = SuballocateFromBuffer(bufferInfo, sizeInBytes, alignment);
    memcpy(pUploadPtr, pData, sizeInBytes);

    commandList.GetGraphicsCommandList()->CopyBufferRegion(
        destination.GetD3D12Resource().Get(),
        destinationOffset,
        bufferInfo.m_Buffer.Get(),
        pUploadPtr - bufferInfo.m_DataBegin,
        sizeInBytes
    );
    commandList.TrackObject(bufferInfo.m_Buffer);
    commandList.TrackResource(destination);
}

SharedUploadBuffer::BufferInfo& SharedUploadBuffer::GetBufferInfoForFrame(const uint64_t frameCount)
{
    return m_BufferInfos[frameCount % BUFFER_COUNT];
}

uint8_t* SharedUploadBuffer::SuballocateFromBuffer(BufferInfo& bufferInfo, const uint64_t size, const uint64_t alignment)
{
    const auto currentAlignedOffset = Math::AlignUp(bufferInfo.GetCurrentUsedSize(), alignment);
    const auto newSize = currentAlignedOffset + size;
    EnsureBufferCapacity(bufferInfo, newSize);
    bufferInfo.m_DataCur = bufferInfo.m_DataBegin + newSize;
    return bufferInfo.m_DataBegin + currentAlignedOffset;
}

void SharedUploadBuffer::EnsureBufferCapacity(BufferInfo& bufferInfo, const uint64_t capacity)
{
    if (bufferInfo.m_Buffer == nullptr || bufferInfo.m_Buffer->GetDesc().Width < capacity)
    {
        const BufferInfo oldBufferInfo = bufferInfo;

        bufferInfo.m_Buffer = CreateBuffer(capacity);

        void* pData;
        const CD3DX12_RANGE readRange(0, 0);
        ThrowIfFailed(bufferInfo.m_Buffer->Map(0, &readRange, &pData));
        bufferInfo.m_DataCur = bufferInfo.m_DataBegin = static_cast<uint8_t*>(pData);
        bufferInfo.m_DataEnd = bufferInfo.m_DataBegin + capacity;

        if (const auto usedSize = oldBufferInfo.GetCurrentUsedSize(); usedSize > 0)
        {
            memcpy(bufferInfo.m_DataBegin, oldBufferInfo.m_DataBegin, usedSize);
        }
    }
}

Microsoft::WRL::ComPtr<ID3D12Resource> SharedUploadBuffer::CreateBuffer(const uint64_t capacity)
{
    const auto pDevice = Application::Get().GetDevice();

    const auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    const auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(capacity);

    Microsoft::WRL::ComPtr<ID3D12Resource> pUploadBuffer = nullptr;
    ThrowIfFailed(pDevice->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&pUploadBuffer)));

    return pUploadBuffer;
}
