#include "DX12LibPCH.h"

#include "CommandList.h"

#include "Application.h"
#include "ByteAddressBuffer.h"
#include "ConstantBuffer.h"
#include "CommandQueue.h"
#include "DynamicDescriptorHeap.h"
#include "IndexBuffer.h"
#include "RenderTarget.h"
#include "Resource.h"
#include "ResourceStateTracker.h"
#include "RootSignature.h"
#include "UploadBuffer.h"
#include "VertexBuffer.h"

#include <DirectXTex.h>

#include <d3d12.h>

#include <filesystem>
#include "StructuredBuffer.h"

std::map<std::wstring, ID3D12Resource*> CommandList::m_TextureCache;
std::mutex CommandList::m_TextureCacheMutex;
namespace fs = std::filesystem;

CommandList::CommandList(D3D12_COMMAND_LIST_TYPE type) : m_D3d12CommandListType(type)
{
    auto device = Application::Get().GetDevice();

    ThrowIfFailed(device->CreateCommandAllocator(m_D3d12CommandListType, IID_PPV_ARGS(&m_D3d12CommandAllocator)));

    ThrowIfFailed(device->CreateCommandList(0, m_D3d12CommandListType, m_D3d12CommandAllocator.Get(),
        nullptr, IID_PPV_ARGS(&m_D3d12CommandList)));

    m_PUploadBuffer = std::make_unique<UploadBuffer>();

    m_PResourceStateTracker = std::make_unique<ResourceStateTracker>();

    for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
    {
        m_DynamicDescriptorHeaps[i] = std::make_unique<DynamicDescriptorHeap>(
            static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(i));
        m_DescriptorHeaps[i] = nullptr;
    }
}

CommandList::~CommandList() = default;

void CommandList::TransitionBarrier(const Resource& resource, const D3D12_RESOURCE_STATES stateAfter,
    const UINT subresource,
    const bool flushBarriers)
{
    TransitionBarrier(resource.GetD3D12Resource(), stateAfter, subresource, flushBarriers);
}

void CommandList::TransitionBarrier(const ComPtr<ID3D12Resource> resource, const D3D12_RESOURCE_STATES stateAfter,
    const UINT subresource,
    const bool flushBarriers)
{
    if (resource)
    {
        const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource.Get(), D3D12_RESOURCE_STATE_COMMON,
            stateAfter, subresource);
        m_PResourceStateTracker->ResourceBarrier(barrier);
    }

    if (flushBarriers)
    {
        FlushResourceBarriers();
    }
}

void CommandList::UavBarrier(const Resource& resource, const bool flushBarriers)
{
    const auto d3d12Resource = resource.GetD3D12Resource();
    const auto barrier = CD3DX12_RESOURCE_BARRIER::UAV(d3d12Resource.Get());

    m_PResourceStateTracker->ResourceBarrier(barrier);

    if (flushBarriers)
    {
        FlushResourceBarriers();
    }
}

void CommandList::AliasingBarrier(const Resource& beforeResource, const Resource& afterResource,
    const bool flushBarriers)
{
    AliasingBarrier(beforeResource.GetD3D12Resource(), afterResource.GetD3D12Resource(), flushBarriers);
}

void CommandList::AliasingBarrier(const ComPtr<ID3D12Resource> beforeResource,
    const ComPtr<ID3D12Resource> afterResource,
    const bool flushBarriers)
{
    const auto barrier = CD3DX12_RESOURCE_BARRIER::Aliasing(beforeResource.Get(), afterResource.Get());

    m_PResourceStateTracker->ResourceBarrier(barrier);

    if (flushBarriers)
    {
        FlushResourceBarriers();
    }
}

void CommandList::FlushResourceBarriers()
{
    m_PResourceStateTracker->FlushResourceBarriers(*this);
}

void CommandList::CopyResource(const Resource& dstRes, const Resource& srcRes)
{
    CopyResource(dstRes.GetD3D12Resource(), srcRes.GetD3D12Resource());
}

void CommandList::CopyResource(const ComPtr<ID3D12Resource> dstRes, const ComPtr<ID3D12Resource> srcRes)
{
    TransitionBarrier(dstRes, D3D12_RESOURCE_STATE_COPY_DEST);
    TransitionBarrier(srcRes, D3D12_RESOURCE_STATE_COPY_SOURCE);

    FlushResourceBarriers();

    m_D3d12CommandList->CopyResource(dstRes.Get(), srcRes.Get());

    TrackObject(dstRes);
    TrackObject(srcRes);
}

void CommandList::ResolveSubresource(const Resource& dstRes, const Resource& srcRes, const uint32_t dstSubresource,
    const uint32_t srcSubresource)
{
    TransitionBarrier(dstRes, D3D12_RESOURCE_STATE_RESOLVE_DEST, dstSubresource);
    TransitionBarrier(srcRes, D3D12_RESOURCE_STATE_RESOLVE_SOURCE, srcSubresource);

    FlushResourceBarriers();

    m_D3d12CommandList->ResolveSubresource(dstRes.GetD3D12Resource().Get(), dstSubresource,
        srcRes.GetD3D12Resource().Get(), srcSubresource,
        dstRes.GetD3D12ResourceDesc().Format);

    TrackResource(srcRes);
    TrackResource(dstRes);
}

void CommandList::CopyBuffer(Buffer& buffer, const size_t numElements, const size_t elementSize, const void* bufferData,
    const D3D12_RESOURCE_FLAGS flags)
{
    const auto device = Application::Get().GetDevice();

    const size_t bufferSize = numElements * elementSize;

    ComPtr<ID3D12Resource> d3d12Resource;
    if (bufferSize == 0)
    {
        // This will result in a NULL resource (which may be desired to define a default null resource).
        throw std::exception();
    }
    else
    {
        const auto desc = buffer.GetD3D12ResourceDesc();

        // see if need to create a new resource
        if (buffer.GetD3D12Resource() == nullptr || desc.Width < bufferSize || desc.Flags != flags)
        {
            const auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
            const auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize, flags);
            ThrowIfFailed(device->CreateCommittedResource(
                &heapProperties,
                D3D12_HEAP_FLAG_NONE,
                &resourceDesc,
                D3D12_RESOURCE_STATE_COMMON,
                nullptr,
                IID_PPV_ARGS(&d3d12Resource)));
            d3d12Resource->SetName(buffer.GetName().c_str());

            // Add the resource to the global resource state tracker.
            ResourceStateTracker::AddGlobalResourceState(d3d12Resource.Get(), D3D12_RESOURCE_STATE_COMMON);
        }
        else // use existing
        {
            d3d12Resource = buffer.GetD3D12Resource();
        }
    }

    if (bufferData != nullptr)
    {
        // Create an upload resource to use as an intermediate buffer to copy the buffer resource
        const auto& uploadResource = buffer.GetUploadResource(bufferSize);

        D3D12_SUBRESOURCE_DATA subresourceData = {};
        subresourceData.pData = bufferData;
        subresourceData.RowPitch = static_cast<LONG_PTR>(bufferSize);
        subresourceData.SlicePitch = subresourceData.RowPitch;

        m_PResourceStateTracker->TransitionResource(d3d12Resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST);
        FlushResourceBarriers();

        UpdateSubresources(m_D3d12CommandList.Get(), d3d12Resource.Get(),
            uploadResource.Get(), 0, 0, 1, &subresourceData);

        // Add references to resources so they stay in scope until the command list is reset.
        TrackObject(uploadResource);
    }
    TrackObject(d3d12Resource);

    buffer.SetD3D12Resource(d3d12Resource);
    buffer.CreateViews(numElements, elementSize);
}

void CommandList::CopyVertexBuffer(VertexBuffer& vertexBuffer, const size_t numVertices, const size_t vertexStride,
    const void* vertexBufferData)
{
    CopyBuffer(vertexBuffer, numVertices, vertexStride, vertexBufferData);
}

void CommandList::CopyIndexBuffer(IndexBuffer& indexBuffer, const size_t numIndices, const DXGI_FORMAT indexFormat,
    const void* indexBufferData)
{
    const size_t indexSizeInBytes = indexFormat == DXGI_FORMAT_R16_UINT ? 2 : 4;
    CopyBuffer(indexBuffer, numIndices, indexSizeInBytes, indexBufferData);
}

void CommandList::CopyByteAddressBuffer(ByteAddressBuffer& byteAddressBuffer, const size_t bufferSize,
    const void* bufferData)
{
    CopyBuffer(byteAddressBuffer, 1, bufferSize, bufferData, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
}

void CommandList::CopyStructuredBuffer(StructuredBuffer& structuredBuffer, size_t numElements, size_t elementSize, const void* bufferData)
{
    CopyBuffer(structuredBuffer, numElements, elementSize, bufferData, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
}

void CommandList::SetPrimitiveTopology(const D3D_PRIMITIVE_TOPOLOGY primitiveTopology) const
{
    m_D3d12CommandList->IASetPrimitiveTopology(primitiveTopology);
}

namespace
{
    DirectX::ScratchImage LoadScratchImage(const fs::path& filePath, DirectX::TexMetadata* pMetadata,
        const bool builtInMipMapGeneration = true)
    {
        DirectX::ScratchImage scratchImage;

        if (filePath.extension() == ".dds")
        {
            ThrowIfFailed(LoadFromDDSFile(filePath.c_str(), DirectX::DDS_FLAGS_FORCE_RGB, pMetadata, scratchImage));
        }
        else if (filePath.extension() == ".hdr")
        {
            ThrowIfFailed(LoadFromHDRFile(filePath.c_str(), pMetadata, scratchImage));
        }
        else if (filePath.extension() == ".tga")
        {
            ThrowIfFailed(LoadFromTGAFile(filePath.c_str(), pMetadata, scratchImage));
        }
        else
        {
            ThrowIfFailed(LoadFromWICFile(filePath.c_str(), DirectX::WIC_FLAGS_FORCE_RGB, pMetadata, scratchImage));
        }

        if (builtInMipMapGeneration && pMetadata->dimension == DirectX::TEX_DIMENSION_TEXTURE2D && !DirectX::IsCompressed(pMetadata->format))
        {
            DirectX::ScratchImage mipChain;
            ThrowIfFailed(GenerateMipMaps(scratchImage.GetImages(), scratchImage.GetImageCount(), *pMetadata,
                DirectX::TEX_FILTER_DEFAULT, 0, mipChain));
            return mipChain;
        }

        return scratchImage;
    }
}

bool CommandList::LoadTextureFromFile(Texture& texture, const std::wstring& fileName,
    const TextureUsageType textureUsage, bool throwOnNotFound)
{
    std::wstring effectiveFileName = fileName;
    fs::path filePath(effectiveFileName);

    // if not dds, try loading dds instead
    if (filePath.extension() != ".dds")
    {
        fs::path ddsFilePath = filePath;
        ddsFilePath.replace_extension(".dds");
        if (exists(ddsFilePath))
        {
            filePath = ddsFilePath;
            effectiveFileName = ddsFilePath;
        }
    }

    if (!exists(filePath))
    {
        if (throwOnNotFound)
            throw std::exception("File not found.");
        return false;
    }

    std::lock_guard lock(m_TextureCacheMutex);
    const auto iter = m_TextureCache.find(effectiveFileName);
    if (iter != m_TextureCache.end())
    {
        texture.SetTextureUsage(textureUsage);
        texture.SetD3D12Resource(iter->second);
        texture.CreateViews();
        texture.SetName(effectiveFileName);
    }
    else
    {
        DirectX::TexMetadata metadata{};
        auto scratchImage = LoadScratchImage(filePath, &metadata);

        if (textureUsage == TextureUsageType::Albedo)
        {
            metadata.format = DirectX::MakeSRGB(metadata.format);
        }

        D3D12_RESOURCE_DESC textureDesc = {};
        switch (metadata.dimension)
        {
        case DirectX::TEX_DIMENSION_TEXTURE1D:
            textureDesc = CD3DX12_RESOURCE_DESC::Tex1D(
                metadata.format, metadata.width, static_cast<UINT16>(metadata.arraySize));
            break;
        case DirectX::TEX_DIMENSION_TEXTURE2D:
            textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(
                metadata.format, metadata.width, metadata.height, static_cast<UINT16>(metadata.arraySize));
            break;
        case DirectX::TEX_DIMENSION_TEXTURE3D:
            textureDesc = CD3DX12_RESOURCE_DESC::Tex3D(
                metadata.format, metadata.width, metadata.height, metadata.depth);
            break;
        default:
            throw std::exception("Invalid texture dimension.");
        }

        const auto device = Application::Get().GetDevice();
        ComPtr<ID3D12Resource> textureResource;

        const auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        constexpr auto initialResourceState = D3D12_RESOURCE_STATE_COMMON;
        ThrowIfFailed(device->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &textureDesc,
            initialResourceState,
            nullptr,
            IID_PPV_ARGS(&textureResource)
        ));

        texture.SetTextureUsage(textureUsage);
        texture.SetD3D12Resource(textureResource);
        texture.CreateViews();
        texture.SetName(effectiveFileName);

        ResourceStateTracker::AddGlobalResourceState(textureResource.Get(), initialResourceState);

        std::vector<D3D12_SUBRESOURCE_DATA> subresources(scratchImage.GetImageCount());
        const DirectX::Image* pImages = scratchImage.GetImages();
        for (int i = 0; i < scratchImage.GetImageCount(); ++i)
        {
            auto& subresourceData = subresources[i];
            subresourceData.RowPitch = pImages[i].rowPitch;
            subresourceData.SlicePitch = pImages[i].slicePitch;
            subresourceData.pData = pImages[i].pixels;
        }

        auto numSubresources = static_cast<uint32_t>(subresources.size());
        CopyTextureSubresource(
            texture,
            0,
            numSubresources,
            subresources.data()
        );

        // Not used when having built-in mipmap generation from DirectXTex
        if (subresources.size() < textureResource->GetDesc().MipLevels)
        {
            GenerateMips(texture);
        }

        m_TextureCache[effectiveFileName] = textureResource.Get();
    }

    return true;
}

void CommandList::ClearTexture(const Texture& texture, const float clearColor[4])
{
    TransitionBarrier(texture, D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_D3d12CommandList->ClearRenderTargetView(texture.GetRenderTargetView(), clearColor, 0, nullptr);

    TrackResource(texture);
}

void CommandList::ClearTexture(const Texture& texture, const ClearValue& clearValue)
{
    ClearTexture(texture, clearValue.GetColor());
}

void CommandList::ClearDepthStencilTexture(const Texture& texture, const D3D12_CLEAR_FLAGS clearFlags,
    const float depth, const uint8_t stencil)
{
    TransitionBarrier(texture, D3D12_RESOURCE_STATE_DEPTH_WRITE);
    m_D3d12CommandList->ClearDepthStencilView(texture.GetDepthStencilView(), clearFlags, depth, stencil, 0, nullptr);

    TrackResource(texture);
}

void CommandList::GenerateMips(Texture& texture)
{
    if (m_D3d12CommandListType == D3D12_COMMAND_LIST_TYPE_COPY)
    {
        if (!m_ComputeCommandList)
        {
            m_ComputeCommandList = Application::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COMPUTE)->
                GetCommandList();
        }
        m_ComputeCommandList->GenerateMips(texture);
        return;
    }

    const auto resource = texture.GetD3D12Resource();

    // If the texture doesn't have a valid resource, do nothing.
    if (!resource) return;
    const auto resourceDesc = resource->GetDesc();

    // If the texture only has a single mip level (level 0)
    // do nothing.
    if (resourceDesc.MipLevels == 1) return;
    // Currently, only non-multi-sampled 2D textures are supported.
    if (resourceDesc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D ||
        resourceDesc.DepthOrArraySize != 1 ||
        resourceDesc.SampleDesc.Count > 1)
    {
        throw std::exception("GenerateMips is only supported for non-multi-sampled 2D Textures.");
    }

    ComPtr<ID3D12Resource> uavResource = resource;
    // Create an alias of the original resource.
    // This is done to perform a GPU copy of resources with different formats.
    // BGR -> RGB texture copies will fail GPU validation unless performed
    // through an alias of the BRG resource in a placed heap.
    ComPtr<ID3D12Resource> aliasResource;

    // If the passed-in resource does not allow for UAV access
    // then create a staging resource that is used to generate
    // the mipmap chain.
    if (!texture.CheckUavSupport() ||
        (resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) == 0)
    {
        const auto device = Application::Get().GetDevice();

        // Describe an alias resource that is used to copy the original texture.
        auto aliasDesc = resourceDesc;
        // Placed resources can't be render targets or depth-stencil views.
        aliasDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        aliasDesc.Flags &= ~(D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

        // Describe a UAV compatible resource that is used to perform
        // mipmapping of the original texture.
        auto uavDesc = aliasDesc; // The flags for the UAV description must match that of the alias description.
        uavDesc.Format = Texture::GetUavCompatibleFormat(resourceDesc.Format);

        D3D12_RESOURCE_DESC resourceDescs[] = {
            aliasDesc,
            uavDesc
        };

        // Create a heap that is large enough to store a copy of the original resource.
        const auto allocationInfo = device->GetResourceAllocationInfo(0, _countof(resourceDescs), resourceDescs);

        D3D12_HEAP_DESC heapDesc = {};
        heapDesc.SizeInBytes = allocationInfo.SizeInBytes;
        heapDesc.Alignment = allocationInfo.Alignment;
        heapDesc.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES;
        heapDesc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapDesc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapDesc.Properties.Type = D3D12_HEAP_TYPE_DEFAULT;

        ComPtr<ID3D12Heap> heap;
        ThrowIfFailed(device->CreateHeap(&heapDesc, IID_PPV_ARGS(&heap)));

        // Make sure the heap does not go out of scope until the command list
        // is finished executing on the command queue.
        TrackObject(heap);

        // Create a placed resource that matches the description of the
        // original resource. This resource is used to copy the original
        // texture to the UAV compatible resource.
        ThrowIfFailed(device->CreatePlacedResource(
            heap.Get(),
            0,
            &aliasDesc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&aliasResource)
        ));

        ResourceStateTracker::AddGlobalResourceState(aliasResource.Get(), D3D12_RESOURCE_STATE_COMMON);
        // Ensure the scope of the alias resource.
        TrackObject(aliasResource);

        // Create a UAV compatible resource in the same heap as the alias
        // resource.
        ThrowIfFailed(device->CreatePlacedResource(
            heap.Get(),
            0,
            &uavDesc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&uavResource)
        ));

        ResourceStateTracker::AddGlobalResourceState(uavResource.Get(), D3D12_RESOURCE_STATE_COMMON);
        // Ensure the scope of the UAV compatible resource.
        TrackObject(uavResource);

        // Add an aliasing barrier for the alias resource.
        AliasingBarrier(nullptr, aliasResource);

        // Copy the original resource to the alias resource.
        // This ensures GPU validation.
        CopyResource(aliasResource, resource);

        // Add an aliasing barrier for the UAV compatible resource.
        AliasingBarrier(aliasResource, uavResource);
    }

    // Generate mips with the UAV compatible resource.
    auto uavTexture = Texture(uavResource, texture.GetTextureUsage());
    GenerateMipsUav(uavTexture, resourceDesc.Format);

    if (aliasResource)
    {
        AliasingBarrier(uavResource, aliasResource);
        // Copy the alias resource back to the original resource.
        CopyResource(resource, aliasResource);
    }
}

void CommandList::CopyTextureSubresource(const Texture& texture, const uint32_t firstSubresource,
    const uint32_t numSubresources,
    D3D12_SUBRESOURCE_DATA* subresourceData)
{
    const auto device = Application::Get().GetDevice();
    const auto destinationResource = texture.GetD3D12Resource();
    if (!destinationResource)
    {
        return;
    }

    TransitionBarrier(texture, D3D12_RESOURCE_STATE_COPY_DEST);
    FlushResourceBarriers();

    const UINT64 requiredSize = GetRequiredIntermediateSize(destinationResource.Get(), firstSubresource,
        numSubresources);

    ComPtr<ID3D12Resource> intermediateResource;
    {
        const auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        const auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(requiredSize);
        ThrowIfFailed(device->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&intermediateResource)
        ));
    }

    UpdateSubresources(m_D3d12CommandList.Get(),
        destinationResource.Get(),
        intermediateResource.Get(),
        0u,
        static_cast<UINT>(firstSubresource),
        static_cast<UINT>(numSubresources),
        subresourceData
    );

    TrackObject(intermediateResource);
    TrackObject(destinationResource);
}

void CommandList::SetGraphicsDynamicConstantBuffer(const uint32_t rootParameterIndex, const size_t sizeInBytes,
    const void* bufferData) const
{
    const auto heapAllocation = m_PUploadBuffer->Allocate(sizeInBytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
    memcpy(heapAllocation.Cpu, bufferData, sizeInBytes);

    m_D3d12CommandList->SetGraphicsRootConstantBufferView(rootParameterIndex, heapAllocation.Gpu);
}

void CommandList::SetComputeDynamicConstantBuffer(uint32_t rootParameterIndex, size_t sizeInBytes, const void* bufferData) const
{
    const auto heapAllocation = m_PUploadBuffer->Allocate(sizeInBytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
    memcpy(heapAllocation.Cpu, bufferData, sizeInBytes);

    m_D3d12CommandList->SetComputeRootConstantBufferView(rootParameterIndex, heapAllocation.Gpu);
}

void CommandList::SetGraphics32BitConstants(const uint32_t rootParameterIndex, const uint32_t numConstants,
    const void* constants)
{
    m_D3d12CommandList->SetGraphicsRoot32BitConstants(rootParameterIndex, numConstants, constants, 0);
}

void CommandList::SetCompute32BitConstants(const uint32_t rootParameterIndex, const uint32_t numConstants,
    const void* constants)
{
    m_D3d12CommandList->SetComputeRoot32BitConstants(rootParameterIndex, numConstants, constants, 0);
}

void CommandList::SetVertexBuffer(const uint32_t slot, const VertexBuffer& vertexBuffer)
{
    TransitionBarrier(vertexBuffer, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

    const auto vertexBufferView = vertexBuffer.GetVertexBufferView();

    m_D3d12CommandList->IASetVertexBuffers(slot, 1, &vertexBufferView);

    TrackResource(vertexBuffer);
}

void CommandList::SetIndexBuffer(const IndexBuffer& indexBuffer)
{
    TransitionBarrier(indexBuffer, D3D12_RESOURCE_STATE_INDEX_BUFFER);

    const auto indexBufferView = indexBuffer.GetIndexBufferView();

    m_D3d12CommandList->IASetIndexBuffer(&indexBufferView);

    TrackResource(indexBuffer);
}

void CommandList::SetGraphicsDynamicStructuredBuffer(const uint32_t slot, const size_t numElements,
    const size_t elementSize,
    const void* bufferData) const
{
    const size_t bufferSize = numElements * elementSize;
    const auto heapAllocation = m_PUploadBuffer->Allocate(bufferSize, elementSize);
    memcpy(heapAllocation.Cpu, bufferData, bufferSize);
    m_D3d12CommandList->SetGraphicsRootShaderResourceView(slot, heapAllocation.Gpu);
}

void CommandList::SetViewport(const D3D12_VIEWPORT& viewport)
{
    SetViewports({ viewport });
}

void CommandList::SetViewports(const std::vector<D3D12_VIEWPORT>& viewports)
{
    assert(viewports.size() < D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE);
    m_D3d12CommandList->RSSetViewports(static_cast<UINT>(viewports.size()),
        viewports.data());
}

void CommandList::SetScissorRect(const D3D12_RECT& scissorRect)
{
    SetScissorRects({ scissorRect });
}

void CommandList::SetScissorRects(const std::vector<D3D12_RECT>& scissorRects)
{
    assert(scissorRects.size() < D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE);
    m_D3d12CommandList->RSSetScissorRects(static_cast<UINT>(scissorRects.size()),
        scissorRects.data());
}

void CommandList::SetPipelineState(const Microsoft::WRL::ComPtr<ID3D12PipelineState>& pipelineState)
{
    m_D3d12CommandList->SetPipelineState(pipelineState.Get());

    TrackObject(pipelineState);
}

void CommandList::SetGraphicsRootSignature(const RootSignature& rootSignature)
{
    const auto d3d12RootSignature = rootSignature.GetRootSignature().Get();
    if (m_RootSignature != d3d12RootSignature)
    {
        m_RootSignature = d3d12RootSignature;

        for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
        {
            m_DynamicDescriptorHeaps[i]->ParseRootSignature(rootSignature);
        }

        m_D3d12CommandList->SetGraphicsRootSignature(m_RootSignature);

        TrackObject(m_RootSignature);
    }
}

void CommandList::SetComputeRootSignature(const RootSignature& rootSignature)
{
    const auto d3d12RootSignature = rootSignature.GetRootSignature().Get();
    if (m_RootSignature != d3d12RootSignature)
    {
        m_RootSignature = d3d12RootSignature;

        for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
        {
            m_DynamicDescriptorHeaps[i]->ParseRootSignature(rootSignature);
        }

        m_D3d12CommandList->SetComputeRootSignature(m_RootSignature);

        TrackObject(m_RootSignature);
    }
}

void CommandList::SetGraphicsAndComputeRootSignature(const RootSignature& rootSignature)
{
    const auto d3d12RootSignature = rootSignature.GetRootSignature().Get();
    if (m_RootSignature != d3d12RootSignature)
    {
        m_RootSignature = d3d12RootSignature;

        for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
        {
            m_DynamicDescriptorHeaps[i]->ParseRootSignature(rootSignature);
        }

        m_D3d12CommandList->SetGraphicsRootSignature(m_RootSignature);
        m_D3d12CommandList->SetComputeRootSignature(m_RootSignature);

        TrackObject(m_RootSignature);
    }
}

void CommandList::SetShaderResourceView(const uint32_t rootParameterIndex, const uint32_t descriptorOffset,
    const Resource& resource, const D3D12_RESOURCE_STATES stateAfter,
    const UINT firstSubresource, const UINT numSubresources,
    const D3D12_SHADER_RESOURCE_VIEW_DESC* srv)
{
    if (numSubresources < D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
    {
        for (uint32_t i = 0; i < numSubresources; ++i)
        {
            TransitionBarrier(resource, stateAfter, firstSubresource + i);
        }
    }
    else
    {
        TransitionBarrier(resource, stateAfter);
    }

    m_DynamicDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->StageDescriptors(
        rootParameterIndex, descriptorOffset, 1, resource.GetShaderResourceView(srv));
    TrackResource(resource);
}

void CommandList::SetUnorderedAccessView(const uint32_t rootParameterIndex, const uint32_t descriptorOffset,
    const Resource& resource, const D3D12_RESOURCE_STATES stateAfter,
    const UINT firstSubresource, const UINT numSubresources,
    const D3D12_UNORDERED_ACCESS_VIEW_DESC* uavDesc)
{
    if (numSubresources < D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
    {
        for (uint32_t i = 0; i < numSubresources; ++i)
        {
            TransitionBarrier(resource, stateAfter, firstSubresource + i);
        }
    }
    else
    {
        TransitionBarrier(resource, stateAfter);
    }

    const auto uav = resource.GetUnorderedAccessView(uavDesc);
    m_DynamicDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->StageDescriptors(
        rootParameterIndex, descriptorOffset, 1, uav);
    TrackResource(resource);
}

void CommandList::SetStencilRef(UINT8 stencilRef)
{
    m_D3d12CommandList->OMSetStencilRef(stencilRef);
}

void CommandList::SetRenderTarget(const RenderTarget& renderTarget, UINT texArrayIndex /*= -1*/, UINT mipLevel /*= 0*/, bool useDepth /*= true*/)
{
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> renderTargetDescriptors;
    renderTargetDescriptors.reserve(NumAttachmentPoints);

    const auto& textures = renderTarget.GetTextures();
    const bool isArrayItem = texArrayIndex != -1;

    // Bind color targets (max of 8 render targets can be bound to the rendering pipeline.
    for (int i = 0; i < 8; ++i)
    {
        auto& texture = textures[i];

        if (texture->IsValid())
        {
            const UINT subresource = isArrayItem ? texture->GetRenderTargetSubresourceIndex(texArrayIndex, mipLevel) : D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            TransitionBarrier(*texture, D3D12_RESOURCE_STATE_RENDER_TARGET, subresource);
            renderTargetDescriptors.push_back(isArrayItem
                ? texture->GetRenderTargetViewArray(texArrayIndex, mipLevel)
                : texture->GetRenderTargetView());

            TrackResource(*texture);
        }
    }

    const auto& depthTexture = renderTarget.GetTexture(DepthStencil);

    CD3DX12_CPU_DESCRIPTOR_HANDLE depthStencilDescriptor(D3D12_DEFAULT);
    if (useDepth && depthTexture->GetD3D12Resource())
    {
        const UINT subresource = isArrayItem ? depthTexture->GetDepthStencilSubresourceIndex(texArrayIndex) : D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        TransitionBarrier(*depthTexture, D3D12_RESOURCE_STATE_DEPTH_WRITE, subresource);
        depthStencilDescriptor = isArrayItem
            ? depthTexture->GetDepthStencilViewArray(texArrayIndex)
            : depthTexture->GetDepthStencilView();

        TrackResource(*depthTexture);
    }

    const D3D12_CPU_DESCRIPTOR_HANDLE* pDsv = depthStencilDescriptor.ptr != 0 ? &depthStencilDescriptor : nullptr;

    m_D3d12CommandList->OMSetRenderTargets(static_cast<UINT>(renderTargetDescriptors.size()),
        renderTargetDescriptors.data(), FALSE, pDsv);

    m_LastRenderTargetState = RenderTargetState(renderTarget);
}

void CommandList::ClearRenderTarget(const RenderTarget& renderTarget, const float* clearColor, D3D12_CLEAR_FLAGS clearFlags)
{
    const auto& textures = renderTarget.GetTextures();

    for (int i = 0; i < 8; ++i)
    {
        auto& texture = textures[i];

        if (texture->IsValid())
        {
            ClearTexture(*texture, clearColor);
        }
    }

    const auto& depthTexture = renderTarget.GetTexture(DepthStencil);
    if (depthTexture->IsValid())
    {
        ClearDepthStencilTexture(*depthTexture, clearFlags);
    }
}

void CommandList::ClearRenderTarget(const RenderTarget& renderTarget, const ClearValue& clearColor, D3D12_CLEAR_FLAGS clearFlags)
{
    ClearRenderTarget(renderTarget, clearColor.GetColor(), clearFlags);
}

void CommandList::Draw(const uint32_t vertexCount, const uint32_t instanceCount, const uint32_t startVertex,
    const uint32_t startInstance)
{
    FlushResourceBarriers();

    for (const auto& dynamicDescriptorHeap : m_DynamicDescriptorHeaps)
    {
        dynamicDescriptorHeap->CommitStagedDescriptorsForDraw(*this);
    }

    m_D3d12CommandList->DrawInstanced(vertexCount, instanceCount, startVertex, startInstance);
}

void CommandList::DrawIndexed(const uint32_t indexCount, const uint32_t instanceCount, const uint32_t startIndex,
    const int32_t baseVertex,
    const uint32_t startInstance)
{
    FlushResourceBarriers();

    for (const auto& dynamicDescriptorHeap : m_DynamicDescriptorHeaps)
    {
        dynamicDescriptorHeap->CommitStagedDescriptorsForDraw(*this);
    }

    m_D3d12CommandList->DrawIndexedInstanced(indexCount, instanceCount, startIndex, baseVertex, startInstance);
}

void CommandList::Dispatch(const uint32_t numGroupsX, const uint32_t numGroupsY, const uint32_t numGroupsZ)
{
    FlushResourceBarriers();

    for (const auto& dynamicDescriptorHeap : m_DynamicDescriptorHeaps)
    {
        dynamicDescriptorHeap->CommitStagedDescriptorsForDispatch(*this);
    }

    m_D3d12CommandList->Dispatch(numGroupsX, numGroupsY, numGroupsZ);
}

bool CommandList::Close(CommandList& pendingCommandList)
{
    // Flush any remaining barriers.
    FlushResourceBarriers();

    m_D3d12CommandList->Close();

    // Flush pending resource barriers.
    const uint32_t numPendingBarriers = m_PResourceStateTracker->FlushPendingResourceBarriers(pendingCommandList);
    // Commit the final resource state to the global state.
    m_PResourceStateTracker->CommitFinalResourceStates();

    return numPendingBarriers > 0;
}

void CommandList::Close()
{
    FlushResourceBarriers();
    m_D3d12CommandList->Close();
}

void CommandList::Reset()
{
    ThrowIfFailed(m_D3d12CommandAllocator->Reset());
    ThrowIfFailed(m_D3d12CommandList->Reset(m_D3d12CommandAllocator.Get(), nullptr));

    m_PResourceStateTracker->Reset();
    m_PUploadBuffer->Reset();

    ReleaseTrackedObjects();

    for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
    {
        m_DynamicDescriptorHeaps[i]->Reset();
        m_DescriptorHeaps[i] = nullptr;
    }

    m_RootSignature = nullptr;
    m_ComputeCommandList = nullptr;
}

void CommandList::TrackObject(const Microsoft::WRL::ComPtr<ID3D12Object>& object)
{
    m_TrackedObjects.push_back(object);
}

void CommandList::TrackResource(const Resource& res)
{
    TrackObject(res.GetD3D12Resource());
}

void CommandList::GenerateMipsUav(Texture& texture, DXGI_FORMAT format)
{
    if (!m_GenerateMipsPso)
    {
        m_GenerateMipsPso = std::make_unique<GenerateMipsPso>();
    }

    m_D3d12CommandList->SetPipelineState(m_GenerateMipsPso->GetPipelineState().Get());
    SetComputeRootSignature(m_GenerateMipsPso->GetRootSignature());

    GenerateMipsCb generateMipsCB;
    generateMipsCB.IsSRgb = Texture::IsSRgbFormat(format);

    auto resource = texture.GetD3D12Resource();
    const auto resourceDesc = resource->GetDesc();

    // Create an SRV that uses the format of the original texture.
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    // Only 2D textures are supported (this was checked in the calling function).
    srvDesc.Texture2D.MipLevels = resourceDesc.MipLevels;

    for (uint32_t srcMip = 0; srcMip < resourceDesc.MipLevels - 1u;)
    {
        uint64_t srcWidth = resourceDesc.Width >> srcMip;
        uint32_t srcHeight = resourceDesc.Height >> srcMip;
        uint32_t dstWidth = static_cast<uint32_t>(srcWidth >> 1);
        uint32_t dstHeight = srcHeight >> 1;

        // 0b00(0): Both width and height are even.
        // 0b01(1): Width is odd, height is even.
        // 0b10(2): Width is even, height is odd.
        // 0b11(3): Both width and height are odd.
        generateMipsCB.SrcDimension = ((srcHeight & 1) << 1) | (srcWidth & 1);

        // How many mipmap levels to compute this pass (max 4 mips per pass)
        DWORD mipCount;

        // The number of times we can half the size of the texture and get
        // exactly a 50% reduction in size.
        // A 1 bit in the width or height indicates an odd dimension.
        // The case where either the width or the height is exactly 1 is handled
        // as a special case (as the dimension does not require reduction).
        _BitScanForward(&mipCount, (dstWidth == 1 ? dstHeight : dstWidth) |
            (dstHeight == 1 ? dstWidth : dstHeight));
        // Maximum number of mips to generate is 4.
        mipCount = std::min<DWORD>(GenerateMipsPso::MAX_MIP_LEVELS_AT_ONCE, mipCount + 1);
        // Clamp to total number of mips left over.
        mipCount = (srcMip + mipCount) >= resourceDesc.MipLevels ? resourceDesc.MipLevels - srcMip - 1 : mipCount;

        // Dimensions should not reduce to 0.
        // This can happen if the width and height are not the same.
        dstWidth = std::max<DWORD>(1, dstWidth);
        dstHeight = std::max<DWORD>(1, dstHeight);

        generateMipsCB.SrcMipLevel = srcMip;
        generateMipsCB.NumMipLevels = mipCount;
        generateMipsCB.TexelSize.x = 1.0f / static_cast<float>(dstWidth);
        generateMipsCB.TexelSize.y = 1.0f / static_cast<float>(dstHeight);

        SetCompute32BitConstants(GenerateMips::GenerateMipsCb, generateMipsCB);

        SetShaderResourceView(GenerateMips::SrcMip, 0, texture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, srcMip,
            1, &srvDesc);

        for (uint32_t mip = 0; mip < mipCount; ++mip)
        {
            D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
            uavDesc.Format = resourceDesc.Format;
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
            uavDesc.Texture2D.MipSlice = srcMip + mip + 1;

            SetUnorderedAccessView(GenerateMips::OutMip, mip, texture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                uavDesc.Texture2D.MipSlice, 1, &uavDesc);
        }

        // Pad any unused mip levels with a default UAV. Doing this keeps the DX12 runtime happy.
        if (mipCount < 4)
        {
            m_DynamicDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->StageDescriptors(
                GenerateMips::OutMip, mipCount, GenerateMipsPso::MAX_MIP_LEVELS_AT_ONCE - mipCount,
                m_GenerateMipsPso->GetDefaultUav());
        }

        Dispatch(Math::DivideByMultiple(dstWidth, 8), Math::DivideByMultiple(dstHeight, 8));

        UavBarrier(texture);

        srcMip += mipCount;
    }
}

void CommandList::ReleaseTrackedObjects()
{
    m_TrackedObjects.clear();
}

void CommandList::SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, ID3D12DescriptorHeap* heap)
{
    if (m_DescriptorHeaps[heapType] != heap)
    {
        m_DescriptorHeaps[heapType] = heap;
        BindDescriptorHeaps();
    }
}

void CommandList::SetComputeRootUnorderedAccessView(UINT rootParameterIndex, const Resource& resource)
{
    auto d3d12Resource = resource.GetD3D12Resource();
    TransitionBarrier(resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    m_D3d12CommandList->SetComputeRootUnorderedAccessView(rootParameterIndex, d3d12Resource->GetGPUVirtualAddress());
    TrackObject(d3d12Resource);
}

void CommandList::SetAutomaticViewportAndScissorRect(const RenderTarget& renderTarget)
{
    const auto& color0Texture = renderTarget.GetTexture(Color0);
    const auto& depthTexture = renderTarget.GetTexture(DepthStencil);
    if (!color0Texture->IsValid() && !depthTexture->IsValid())
    {
        throw std::exception("Both Color0 and DepthStencil attachment are invalid. Cannot compute viewport.");
    }

    auto destinationDesc = color0Texture->IsValid() ? color0Texture->GetD3D12ResourceDesc() : depthTexture->GetD3D12ResourceDesc();
    auto viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(destinationDesc.Width), static_cast<float>(destinationDesc.Height));

    SetViewport(viewport);
    SetInfiniteScrissorRect();
}

void CommandList::SetInfiniteScrissorRect()
{
    auto scissorRect = CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX);
    SetScissorRect(scissorRect);
}

void CommandList::BindDescriptorHeaps()
{
    UINT numDescriptorHeaps = 0;
    ID3D12DescriptorHeap* descriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES] = {};

    for (uint32_t i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
    {
        ID3D12DescriptorHeap* descriptorHeap = m_DescriptorHeaps[i];
        if (descriptorHeap)
        {
            descriptorHeaps[numDescriptorHeaps++] = descriptorHeap;
        }
    }

    m_D3d12CommandList->SetDescriptorHeaps(numDescriptorHeaps, descriptorHeaps);
}
