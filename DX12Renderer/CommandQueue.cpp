// ReSharper disable CppClangTidyClangDiagnosticLanguageExtensionToken
// ReSharper disable CppClangTidyBugproneSizeofExpression
#include "CommandQueue.h"

#include <cassert>

#include "Helpers.h"

using namespace Microsoft::WRL;

CommandQueue::CommandQueue(const ComPtr<ID3D12Device2>& device, const D3D12_COMMAND_LIST_TYPE type) :
	CommandListType(type),
	D3D12Device(device),
	FenceValue(0)
{
	D3D12_COMMAND_QUEUE_DESC desc;
	desc.Type = type;
	desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	desc.NodeMask = 0;

	ThrowIfFailed(D3D12Device->CreateCommandQueue(&desc, IID_PPV_ARGS(&D3D12CommandQueue)));
	ThrowIfFailed(D3D12Device->CreateFence(FenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&D3D12Fence)));

	FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	assert(FenceEvent && "Failed to create fence event handle.");
}

CommandQueue::~CommandQueue() = default;

uint64_t CommandQueue::Signal()
{
	const uint64_t fenceValue = ++FenceValue;
	D3D12CommandQueue->Signal(D3D12Fence.Get(), fenceValue);
	return fenceValue;
}

bool CommandQueue::IsFenceComplete(const uint64_t fenceValue) const
{
	return D3D12Fence->GetCompletedValue() >= fenceValue;
}

void CommandQueue::WaitForFenceValue(const uint64_t fenceValue) const
{
	if (!IsFenceComplete(fenceValue))
	{
		D3D12Fence->SetEventOnCompletion(fenceValue, FenceEvent);
		WaitForSingleObject(FenceEvent, DWORD_MAX);
	}
}

void CommandQueue::Flush()
{
	WaitForFenceValue(Signal());
}

ComPtr<ID3D12CommandQueue> CommandQueue::GetD3D12CommandQueue() const
{
	return D3D12CommandQueue;
}


ComPtr<ID3D12CommandAllocator> CommandQueue::CreateCommandAllocator() const
{
	ComPtr<ID3D12CommandAllocator> commandAllocator;
	ThrowIfFailed(D3D12Device->CreateCommandAllocator(CommandListType, IID_PPV_ARGS(&commandAllocator)));
	return commandAllocator;
}


ComPtr<ID3D12GraphicsCommandList2> CommandQueue::CreateCommandList(
	const ComPtr<ID3D12CommandAllocator>& allocator) const
{
	ComPtr<ID3D12GraphicsCommandList2> commandList;
	ThrowIfFailed(D3D12Device->CreateCommandList(0, CommandListType, allocator.Get(), nullptr,
	                                             IID_PPV_ARGS(&commandList)));
	return commandList;
}


ComPtr<ID3D12GraphicsCommandList2> CommandQueue::GetCommandList()
{
	ComPtr<ID3D12CommandAllocator> commandAllocator;
	ComPtr<ID3D12GraphicsCommandList2> commandList;

	if (!CommandAllocatorQueue.empty() && IsFenceComplete(CommandAllocatorQueue.front().FenceValue))
	{
		commandAllocator = CommandAllocatorQueue.front().CommandAllocator;
		CommandAllocatorQueue.pop();

		ThrowIfFailed(commandAllocator->Reset());
	}
	else
	{
		commandAllocator = CreateCommandAllocator();
	}

	if (!CommandListQueue.empty())
	{
		commandList = CommandListQueue.front();
		CommandListQueue.pop();

		ThrowIfFailed(commandList->Reset(commandAllocator.Get(), nullptr));
	}
	else
	{
		commandList = CreateCommandList(commandAllocator);
	}

	// Associate the command allocator with the command list so that it can be
	// retrieved when the command list is executed.
	ThrowIfFailed(commandList->SetPrivateDataInterface(__uuidof(ID3D12CommandAllocator), commandAllocator.Get()));

	return commandList;
}

uint64_t CommandQueue::ExecuteCommandList(const ComPtr<ID3D12GraphicsCommandList2> commandList)
{
	commandList->Close();

	ID3D12CommandAllocator* commandAllocator;
	UINT dataSize = sizeof(commandAllocator);
	ThrowIfFailed(commandList->GetPrivateData(__uuidof(ID3D12CommandAllocator), &dataSize, &commandAllocator));

	ID3D12CommandList* const ppCommandLists[] = {
		commandList.Get()
	};

	D3D12CommandQueue->ExecuteCommandLists(1, ppCommandLists);
	const uint64_t fenceValue = Signal();

	CommandAllocatorQueue.emplace(CommandAllocatorEntry{fenceValue, commandAllocator});
	CommandListQueue.push(commandList);

	// The ownership of the command allocator has been transferred to the ComPtr
	// in the command allocator queue. It is safe to release the reference 
	// in this temporary COM pointer here.
	commandAllocator->Release();

	return fenceValue;
}
