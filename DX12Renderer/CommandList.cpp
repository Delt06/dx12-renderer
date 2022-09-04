#include "DX12LibPCH.h"

#include "CommandList.h"

#include "Application.h"
#include "ByteAddressBuffer.h"
#include "ConstantBuffer.h"
#include "CommandQueue.h"
#include "DynamicDescriptorHeap.h"
#include "IndexBuffer.h"
#include "RenderTarget.h"
#include "ResourceWrapper.h"
#include "ResourceStateTracker.h"
#include "RootSignature.h"
#include "UploadBuffer.h"
#include "VertexBuffer.h"

std::map<std::wstring, ID3D12Resource*> CommandList::TextureCache;
std::mutex CommandList::TextureCacheMutex;

CommandList::CommandList(D3D12_COMMAND_LIST_TYPE type) : D3d12CommandListType(type)
{
	auto device = Application::Get().GetDevice();

	ThrowIfFailed(device->CreateCommandAllocator(D3d12CommandListType, IID_PPV_ARGS(&D3d12CommandAllocator)));

	ThrowIfFailed(device->CreateCommandList(0, D3d12CommandListType, D3d12CommandAllocator.Get(),
	                                        nullptr, IID_PPV_ARGS(&D3d12CommandList)));

	PUploadBuffer = std::make_unique<UploadBuffer>();

	PResourceStateTracker = std::make_unique<ResourceStateTracker>();

	for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
	{
		DynamicDescriptorHeaps[i] = std::make_unique<DynamicDescriptorHeap>(static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(i));
		DescriptorHeaps[i] = nullptr;
	}
}

CommandList::~CommandList() = default;

void CommandList::TransitionBarrier(const Resource& resource, const D3D12_RESOURCE_STATES stateAfter,
                                    const UINT subresource,
                                    const bool flushBarriers)
{
	const auto d3d12Resource = resource.GetD3D12Resource();
	if (d3d12Resource)
	{
		const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(d3d12Resource.Get(), D3D12_RESOURCE_STATE_COMMON,
		                                                          stateAfter, subresource);
		PResourceStateTracker->ResourceBarrier(barrier);
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

	PResourceStateTracker->ResourceBarrier(barrier);

	if (flushBarriers)
	{
		FlushResourceBarriers();
	}
}

void CommandList::AliasingBarrier(const Resource& beforeResource, const Resource& afterResource,
                                  const bool flushBarriers)
{
	const auto d3d12BeforeResource = beforeResource.GetD3D12Resource();
	const auto d3d12AfterResource = afterResource.GetD3D12Resource();
	const auto barrier = CD3DX12_RESOURCE_BARRIER::Aliasing(d3d12BeforeResource.Get(), d3d12AfterResource.Get());

	PResourceStateTracker->ResourceBarrier(barrier);

	if (flushBarriers)
	{
		FlushResourceBarriers();
	}
}

void CommandList::FlushResourceBarriers()
{
	PResourceStateTracker->FlushResourceBarriers(*this);
}


void CommandList::CopyResource(const Resource& dstRes, const Resource& srcRes)
{
	TransitionBarrier(dstRes, D3D12_RESOURCE_STATE_COPY_DEST);
	TransitionBarrier(srcRes, D3D12_RESOURCE_STATE_COPY_SOURCE);

	FlushResourceBarriers();

	D3d12CommandList->CopyResource(dstRes.GetD3D12Resource().Get(), srcRes.GetD3D12Resource().Get());

	TrackResource(dstRes);
	TrackResource(srcRes);
}

void CommandList::ResolveSubresource(const Resource& dstRes, const Resource& srcRes, const uint32_t dstSubresource,
                                     const uint32_t srcSubresource)
{
	TransitionBarrier(dstRes, D3D12_RESOURCE_STATE_RESOLVE_DEST, dstSubresource);
	TransitionBarrier(srcRes, D3D12_RESOURCE_STATE_RESOLVE_SOURCE, srcSubresource);

	FlushResourceBarriers();

	D3d12CommandList->ResolveSubresource(dstRes.GetD3D12Resource().Get(), dstSubresource,
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
	}
	else
	{
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
		}


		// Add the resource to the global resource state tracker.
		ResourceStateTracker::AddGlobalResourceState(d3d12Resource.Get(), D3D12_RESOURCE_STATE_COMMON);

		if (bufferData != nullptr)
		{
			// Create an upload resource to use as an intermediate buffer to copy the buffer resource 
			ComPtr<ID3D12Resource> uploadResource;
			{
				const auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
				const auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
				ThrowIfFailed(device->CreateCommittedResource(
					&heapProperties,
					D3D12_HEAP_FLAG_NONE,
					&resourceDesc,
					D3D12_RESOURCE_STATE_GENERIC_READ,
					nullptr,
					IID_PPV_ARGS(&uploadResource)));
			}


			D3D12_SUBRESOURCE_DATA subresourceData = {};
			subresourceData.pData = bufferData;
			subresourceData.RowPitch = bufferSize;
			subresourceData.SlicePitch = subresourceData.RowPitch;

			PResourceStateTracker->TransitionResource(d3d12Resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST);
			FlushResourceBarriers();

			UpdateSubresources(D3d12CommandList.Get(), d3d12Resource.Get(),
			                   uploadResource.Get(), 0, 0, 1, &subresourceData);

			// Add references to resources so they stay in scope until the command list is reset.
			TrackObject(uploadResource);
		}
		TrackObject(d3d12Resource);
	}

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

void CommandList::SetPrimitiveTopology(const D3D_PRIMITIVE_TOPOLOGY primitiveTopology)
{
	D3d12CommandList->IASetPrimitiveTopology(primitiveTopology);
}

void CommandList::ClearTexture(const Texture& texture, const float clearColor[4])
{
	TransitionBarrier(texture, D3D12_RESOURCE_STATE_RENDER_TARGET);
	D3d12CommandList->ClearRenderTargetView(texture.GetRenderTargetView(), clearColor, 0, nullptr);

	TrackResource(texture);
}

void CommandList::ClearDepthStencilTexture(const Texture& texture, const D3D12_CLEAR_FLAGS clearFlags,
                                           const float depth, const uint8_t stencil)
{
	TransitionBarrier(texture, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	D3d12CommandList->ClearDepthStencilView(texture.GetDepthStencilView(), clearFlags, depth, stencil, 0, nullptr);

	TrackResource(texture);
}


void CommandList::SetGraphicsDynamicConstantBuffer(const uint32_t rootParameterIndex, const size_t sizeInBytes,
                                                   const void* bufferData) const
{
	const auto heapAllocation = PUploadBuffer->Allocate(sizeInBytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	memcpy(heapAllocation.Cpu, bufferData, sizeInBytes);

	D3d12CommandList->SetGraphicsRootConstantBufferView(rootParameterIndex, heapAllocation.Gpu);
}

void CommandList::SetGraphics32BitConstants(const uint32_t rootParameterIndex, const uint32_t numConstants,
                                            const void* constants)
{
	D3d12CommandList->SetGraphicsRoot32BitConstants(rootParameterIndex, numConstants, constants, 0);
}

void CommandList::SetCompute32BitConstants(const uint32_t rootParameterIndex, const uint32_t numConstants,
                                           const void* constants)
{
	D3d12CommandList->SetComputeRoot32BitConstants(rootParameterIndex, numConstants, constants, 0);
}

void CommandList::SetVertexBuffer(const uint32_t slot, const VertexBuffer& vertexBuffer)
{
	TransitionBarrier(vertexBuffer, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

	const auto vertexBufferView = vertexBuffer.GetVertexBufferView();

	D3d12CommandList->IASetVertexBuffers(slot, 1, &vertexBufferView);

	TrackResource(vertexBuffer);
}

void CommandList::SetIndexBuffer(const IndexBuffer& indexBuffer)
{
	TransitionBarrier(indexBuffer, D3D12_RESOURCE_STATE_INDEX_BUFFER);

	const auto indexBufferView = indexBuffer.GetIndexBufferView();

	D3d12CommandList->IASetIndexBuffer(&indexBufferView);

	TrackResource(indexBuffer);
}

void CommandList::SetViewport(const D3D12_VIEWPORT& viewport)
{
	SetViewports({viewport});
}

void CommandList::SetViewports(const std::vector<D3D12_VIEWPORT>& viewports)
{
	assert(viewports.size() < D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE);
	D3d12CommandList->RSSetViewports(static_cast<UINT>(viewports.size()),
	                                 viewports.data());
}

void CommandList::SetScissorRect(const D3D12_RECT& scissorRect)
{
	SetScissorRects({scissorRect});
}

void CommandList::SetScissorRects(const std::vector<D3D12_RECT>& scissorRects)
{
	assert(scissorRects.size() < D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE);
	D3d12CommandList->RSSetScissorRects(static_cast<UINT>(scissorRects.size()),
	                                    scissorRects.data());
}

void CommandList::SetPipelineState(const ComPtr<ID3D12PipelineState> pipelineState)
{
	D3d12CommandList->SetPipelineState(pipelineState.Get());

	TrackObject(pipelineState);
}

void CommandList::SetGraphicsRootSignature(const RootSignature& rootSignature)
{
	const auto d3d12RootSignature = rootSignature.GetRootSignature().Get();
	if (PRootSignature != d3d12RootSignature)
	{
		PRootSignature = d3d12RootSignature;

		for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
		{
			DynamicDescriptorHeaps[i]->ParseRootSignature(rootSignature);
		}

		D3d12CommandList->SetGraphicsRootSignature(PRootSignature);

		TrackObject(PRootSignature);
	}
}

void CommandList::SetComputeRootSignature(const RootSignature& rootSignature)
{
	const auto d3d12RootSignature = rootSignature.GetRootSignature().Get();
	if (PRootSignature != d3d12RootSignature)
	{
		PRootSignature = d3d12RootSignature;

		for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
		{
			DynamicDescriptorHeaps[i]->ParseRootSignature(rootSignature);
		}

		D3d12CommandList->SetComputeRootSignature(PRootSignature);

		TrackObject(PRootSignature);
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

	DynamicDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->StageDescriptors(
		rootParameterIndex, descriptorOffset, 1, resource.GetShaderResourceView(srv));
	TrackResource(resource);
}

void CommandList::SetUnorderedAccessView(const uint32_t rootParameterIndex, const uint32_t descriptorOffset,
                                         const Resource& resource, const D3D12_RESOURCE_STATES stateAfter,
                                         const UINT firstSubresource, const UINT numSubresources,
                                         const D3D12_UNORDERED_ACCESS_VIEW_DESC* uav)
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

	DynamicDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->StageDescriptors(
		rootParameterIndex, descriptorOffset, 1, resource.GetUnorderedAccessView(uav));
	TrackResource(resource);
}

void CommandList::SetRenderTarget(const RenderTarget& renderTarget)
{
	std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> renderTargetDescriptors;
	renderTargetDescriptors.reserve(NumAttachmentPoints);

	const auto& textures = renderTarget.GetTextures();

	// Bind color targets (max of 8 render targets can be bound to the rendering pipeline.
	for (int i = 0; i < 8; ++i)
	{
		auto& texture = textures[i];

		if (texture.IsValid())
		{
			TransitionBarrier(texture, D3D12_RESOURCE_STATE_RENDER_TARGET);
			renderTargetDescriptors.push_back(texture.GetRenderTargetView());

			TrackResource(texture);
		}
	}

	const auto& depthTexture = renderTarget.GetTexture(DepthStencil);

	CD3DX12_CPU_DESCRIPTOR_HANDLE depthStencilDescriptor(D3D12_DEFAULT);
	if (depthTexture.GetD3D12Resource())
	{
		TransitionBarrier(depthTexture, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		depthStencilDescriptor = depthTexture.GetDepthStencilView();

		TrackResource(depthTexture);
	}

	const D3D12_CPU_DESCRIPTOR_HANDLE* pDsv = depthStencilDescriptor.ptr != 0 ? &depthStencilDescriptor : nullptr;

	D3d12CommandList->OMSetRenderTargets(static_cast<UINT>(renderTargetDescriptors.size()),
	                                     renderTargetDescriptors.data(), FALSE, pDsv);
}

void CommandList::Draw(const uint32_t vertexCount, const uint32_t instanceCount, const uint32_t startVertex,
                       const uint32_t startInstance)
{
	FlushResourceBarriers();

	for (const auto& dynamicDescriptorHeap : DynamicDescriptorHeaps)
	{
		dynamicDescriptorHeap->CommitStagedDescriptorsForDraw(*this);
	}

	D3d12CommandList->DrawInstanced(vertexCount, instanceCount, startVertex, startInstance);
}

void CommandList::DrawIndexed(const uint32_t indexCount, const uint32_t instanceCount, const uint32_t startIndex,
                              const int32_t baseVertex,
                              const uint32_t startInstance)
{
	FlushResourceBarriers();

	for (const auto& dynamicDescriptorHeap : DynamicDescriptorHeaps)
	{
		dynamicDescriptorHeap->CommitStagedDescriptorsForDraw(*this);
	}

	D3d12CommandList->DrawIndexedInstanced(indexCount, instanceCount, startIndex, baseVertex, startInstance);
}


void CommandList::Dispatch(const uint32_t numGroupsX, const uint32_t numGroupsY, const uint32_t numGroupsZ)
{
	FlushResourceBarriers();

	for (const auto& dynamicDescriptorHeap : DynamicDescriptorHeaps)
	{
		dynamicDescriptorHeap->CommitStagedDescriptorsForDispatch(*this);
	}

	D3d12CommandList->Dispatch(numGroupsX, numGroupsY, numGroupsZ);
}

bool CommandList::Close(CommandList& pendingCommandList)
{
	// Flush any remaining barriers.
	FlushResourceBarriers();

	D3d12CommandList->Close();

	// Flush pending resource barriers.
	const uint32_t numPendingBarriers = PResourceStateTracker->FlushPendingResourceBarriers(pendingCommandList);
	// Commit the final resource state to the global state.
	PResourceStateTracker->CommitFinalResourceStates();

	return numPendingBarriers > 0;
}

void CommandList::Close()
{
	FlushResourceBarriers();
	D3d12CommandList->Close();
}

void CommandList::Reset()
{
	ThrowIfFailed(D3d12CommandAllocator->Reset());
	ThrowIfFailed(D3d12CommandList->Reset(D3d12CommandAllocator.Get(), nullptr));

	PResourceStateTracker->Reset();
	PUploadBuffer->Reset();

	ReleaseTrackedObjects();

	for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
	{
		DynamicDescriptorHeaps[i]->Reset();
		DescriptorHeaps[i] = nullptr;
	}

	PRootSignature = nullptr;
	ComputeCommandList = nullptr;
}

void CommandList::TrackObject(const ComPtr<ID3D12Object> object)
{
	TrackedObjects.push_back(object);
}

void CommandList::TrackResource(const Resource& res)
{
	TrackObject(res.GetD3D12Resource());
}

void CommandList::ReleaseTrackedObjects()
{
	TrackedObjects.clear();
}

void CommandList::SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, ID3D12DescriptorHeap* heap)
{
	if (DescriptorHeaps[heapType] != heap)
	{
		DescriptorHeaps[heapType] = heap;
		BindDescriptorHeaps();
	}
}

void CommandList::BindDescriptorHeaps()
{
	UINT numDescriptorHeaps = 0;
	ID3D12DescriptorHeap* descriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES] = {};

	for (uint32_t i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
	{
		ID3D12DescriptorHeap* descriptorHeap = DescriptorHeaps[i];
		if (descriptorHeap)
		{
			descriptorHeaps[numDescriptorHeaps++] = descriptorHeap;
		}
	}

	D3d12CommandList->SetDescriptorHeaps(numDescriptorHeaps, descriptorHeaps);
}
