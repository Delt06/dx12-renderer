#pragma once

/**
 * Wrapper for ID3D12CommandQueue.
 */

#include <d3d12.h>
#include <wrl.h>

#include <cstdint>
#include <queue>

class CommandQueue
{
public:
	CommandQueue(Microsoft::WRL::ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type);
	virtual ~CommandQueue();

	// Get an available command list
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> GetCommandList();

	// Execute a command list
	// Returns the fence value to wait for this command list
	uint64_t ExecuteCommandList(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList);

	uint64_t Signal();
	bool IsFenceComplete(uint64_t fenceValue);
	bool WaitForFenceValue(uint64_t fenceValue);
	void Flush();

	Microsoft::WRL::ComPtr<ID3D12CommandQueue> GetD3D12CommandQueue() const;

protected:
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CreateCommandAllocator();
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> CreateCommandList(
		Microsoft::WRL::ComPtr<ID3D12CommandList> allocator);

private:
	// Keep track of command allocators that are "in-flight"
	struct CommandAllocatorEntry
	{
		uint64_t FenceValue;
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CommandAllocator;
	};

	using CommandAllocatorQueueType = std::queue<CommandAllocatorEntry>;
	using CommandListQueueType = std::queue<Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2>>;

	D3D12_COMMAND_LIST_TYPE CommandListType;
	Microsoft::WRL::ComPtr<ID3D12Device2> D3D12Device;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> D3D12CommandQueue;
	Microsoft::WRL::ComPtr<ID3D12Fence> D3D12Fence;
	HANDLE FenceEvent;
	uint64_t FenceValue;

	CommandAllocatorQueueType CommandAllocatorQueue;
	CommandListQueueType CommandListQueue;
};
