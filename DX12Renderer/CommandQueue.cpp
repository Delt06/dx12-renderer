#include <DX12LibPCH.h>

#include <CommandQueue.h>

#include <Application.h>
#include <CommandList.h>
#include <ResourceStateTracker.h>

CommandQueue::CommandQueue(D3D12_COMMAND_LIST_TYPE type)
	: CommandListType(type)
	  , FenceValue(0)
	  , IsProcessingInFlightCommandLists(true)
{
	auto device = Application::Get().GetDevice();

	D3D12_COMMAND_QUEUE_DESC desc = {};
	desc.Type = type;
	desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	desc.NodeMask = 0;

	ThrowIfFailed(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&D3d12CommandQueue)));
	ThrowIfFailed(device->CreateFence(FenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&D3d12Fence)));

	switch (type)
	{
	case D3D12_COMMAND_LIST_TYPE_COPY:
		D3d12CommandQueue->SetName(L"Copy Command Queue");
		break;
	case D3D12_COMMAND_LIST_TYPE_COMPUTE:
		D3d12CommandQueue->SetName(L"Compute Command Queue");
		break;
	case D3D12_COMMAND_LIST_TYPE_DIRECT:
		D3d12CommandQueue->SetName(L"Direct Command Queue");
		break;
	}

	ProcessInFlightCommandListsThread = std::thread(&CommandQueue::ProcessInFlightCommandLists, this);
}

CommandQueue::~CommandQueue()
{
	IsProcessingInFlightCommandLists = false;
	ProcessInFlightCommandListsThread.join();
}

uint64_t CommandQueue::Signal()
{
	uint64_t fenceValue = ++FenceValue;
	D3d12CommandQueue->Signal(D3d12Fence.Get(), fenceValue);
	return fenceValue;
}

bool CommandQueue::IsFenceComplete(uint64_t fenceValue)
{
	return D3d12Fence->GetCompletedValue() >= fenceValue;
}

void CommandQueue::WaitForFenceValue(uint64_t fenceValue)
{
	if (!IsFenceComplete(fenceValue))
	{
		auto event = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
		assert(event && "Failed to create fence event handle.");

		// Is this function thread safe?
		D3d12Fence->SetEventOnCompletion(fenceValue, event);
		WaitForSingleObject(event, DWORD_MAX);

		CloseHandle(event);
	}
}

void CommandQueue::Flush()
{
	std::unique_lock<std::mutex> lock(ProcessInFlightCommandListsThreadMutex);
	ProcessInFlightCommandListsThreadCv.wait(lock, [this] { return InFlightCommandLists.Empty(); });

	// In case the command queue was signaled directly 
	// using the CommandQueue::Signal method then the 
	// fence value of the command queue might be higher than the fence
	// value of any of the executed command lists.
	WaitForFenceValue(FenceValue);
}

std::shared_ptr<CommandList> CommandQueue::GetCommandList()
{
	std::shared_ptr<CommandList> commandList;

	// If there is a command list on the queue.
	if (!AvailableCommandLists.Empty())
	{
		AvailableCommandLists.TryPop(commandList);
	}
	else
	{
		// Otherwise create a new command list.
		commandList = std::make_shared<CommandList>(CommandListType);
	}

	return commandList;
}

// Execute a command list.
// Returns the fence value to wait for for this command list.
uint64_t CommandQueue::ExecuteCommandList(std::shared_ptr<CommandList> commandList)
{
	return ExecuteCommandLists(std::vector<std::shared_ptr<CommandList>>({commandList}));
}

uint64_t CommandQueue::ExecuteCommandLists(const std::vector<std::shared_ptr<CommandList>>& commandLists)
{
	ResourceStateTracker::Lock();

	// Command lists that need to put back on the command list queue.
	std::vector<std::shared_ptr<CommandList>> toBeQueued;
	toBeQueued.reserve(commandLists.size() * 2); // 2x since each command list will have a pending command list.

	// Generate mips command lists.
	std::vector<std::shared_ptr<CommandList>> generateMipsCommandLists;
	generateMipsCommandLists.reserve(commandLists.size());

	// Command lists that need to be executed.
	std::vector<ID3D12CommandList*> d3d12CommandLists;
	d3d12CommandLists.reserve(commandLists.size() * 2); // 2x since each command list will have a pending command list.

	for (auto commandList : commandLists)
	{
		auto pendingCommandList = GetCommandList();
		bool hasPendingBarriers = commandList->Close(*pendingCommandList);
		pendingCommandList->Close();
		// If there are no pending barriers on the pending command list, there is no reason to 
		// execute an empty command list on the command queue.
		if (hasPendingBarriers)
		{
			d3d12CommandLists.push_back(pendingCommandList->GetGraphicsCommandList().Get());
		}
		d3d12CommandLists.push_back(commandList->GetGraphicsCommandList().Get());

		toBeQueued.push_back(pendingCommandList);
		toBeQueued.push_back(commandList);

		auto generateMipsCommandList = commandList->GetGenerateMipsCommandList();
		if (generateMipsCommandList)
		{
			generateMipsCommandLists.push_back(generateMipsCommandList);
		}
	}

	UINT numCommandLists = static_cast<UINT>(d3d12CommandLists.size());
	D3d12CommandQueue->ExecuteCommandLists(numCommandLists, d3d12CommandLists.data());
	uint64_t fenceValue = Signal();

	ResourceStateTracker::Unlock();

	// Queue command lists for reuse.
	for (auto commandList : toBeQueued)
	{
		InFlightCommandLists.Push({fenceValue, commandList});
	}

	// If there are any command lists that generate mips then execute those
	// after the initial resource command lists have finished.
	if (generateMipsCommandLists.size() > 0)
	{
		auto computeQueue = Application::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COMPUTE);
		computeQueue->Wait(*this);
		computeQueue->ExecuteCommandLists(generateMipsCommandLists);
	}

	return fenceValue;
}

void CommandQueue::Wait(const CommandQueue& other)
{
	D3d12CommandQueue->Wait(other.D3d12Fence.Get(), other.FenceValue);
}

ComPtr<ID3D12CommandQueue> CommandQueue::GetD3D12CommandQueue() const
{
	return D3d12CommandQueue;
}

void CommandQueue::ProcessInFlightCommandLists()
{
	std::unique_lock<std::mutex> lock(ProcessInFlightCommandListsThreadMutex, std::defer_lock);

	while (IsProcessingInFlightCommandLists)
	{
		CommandListEntry commandListEntry;

		lock.lock();
		while (InFlightCommandLists.TryPop(commandListEntry))
		{
			const auto fenceValue = std::get<0>(commandListEntry);
			auto commandList = std::get<1>(commandListEntry);

			WaitForFenceValue(fenceValue);

			commandList->Reset();

			AvailableCommandLists.Push(commandList);
		}
		lock.unlock();
		ProcessInFlightCommandListsThreadCv.notify_one();

		std::this_thread::yield();
	}
}
