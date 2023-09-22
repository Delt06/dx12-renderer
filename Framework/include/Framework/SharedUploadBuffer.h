#pragma once

#include <cstdint>
#include <vector>

#include <d3d12.h>
#include <wrl.h>

#include <DX12Library/CommandList.h>
#include <DX12Library/Window.h>

class SharedUploadBuffer
{
public:
    // Resets the pointer of the current buffer.
    void BeginFrame();

    template <typename T>
    void Upload(CommandList& commandList, Resource& destination, const std::vector<T>& data, const uint64_t destinationOffset = 0U)
    {
        Upload(commandList, destination, data.data(), data.size() * sizeof(T), sizeof(T), destinationOffset);
    }

    void Upload(CommandList& commandList, const Resource& destination, const void* pData, uint64_t sizeInBytes, uint64_t alignment, uint64_t destinationOffset = 0U);

private:
    static constexpr auto BUFFER_COUNT = Window::BUFFER_COUNT;
    static constexpr uint64_t CAPACITY_ALIGNMENT = 102400;



    struct BufferInfo
    {
        Microsoft::WRL::ComPtr<ID3D12Resource> m_Buffer = nullptr;
        uint8_t* m_DataBegin = nullptr;
        uint8_t* m_DataCur = nullptr;
        uint8_t* m_DataEnd = nullptr;

        uint64_t GetCurrentUsedSize() const
        {
            if (m_Buffer == nullptr)
            {
                return 0;
            }
            return m_DataCur - m_DataBegin;
        }
    };

    BufferInfo& GetBufferInfoForFrame(uint64_t frameCount);

    static uint8_t* SuballocateFromBuffer(BufferInfo& bufferInfo, uint64_t size, uint64_t alignment);

    static void EnsureBufferCapacity(BufferInfo& bufferInfo, uint64_t capacity);

    static Microsoft::WRL::ComPtr<ID3D12Resource> CreateBuffer(uint64_t capacity);

    BufferInfo m_BufferInfos[BUFFER_COUNT];
};
