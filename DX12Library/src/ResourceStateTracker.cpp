#include "DX12LibPCH.h"

#include "ResourceStateTracker.h"

#include "CommandList.h"
#include "Resource.h"

#include <d3d12.h>
#include <d3dx12.h>

std::mutex ResourceStateTracker::s_GlobalMutex;
bool ResourceStateTracker::s_IsLocked;
ResourceStateTracker::ResourceStateMapType ResourceStateTracker::s_GlobalResourceStates;

ResourceStateTracker::ResourceStateTracker() = default;

ResourceStateTracker::~ResourceStateTracker() = default;

void ResourceStateTracker::ResourceBarrier(const D3D12_RESOURCE_BARRIER& barrier)
{
	if (barrier.Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION)
	{
		const D3D12_RESOURCE_TRANSITION_BARRIER& transitionBarrier = barrier.Transition;

		// First check if there is already a known "final" state for the given resource.
		// If there is, the resource has been used on the command list before and
		// already has a known state within the command list execution.
		const auto iter = m_FinalResourceStates.find(transitionBarrier.pResource);
		if (iter != m_FinalResourceStates.end())
		{
			const auto& resourceState = iter->second;
			// If the known final state of the resource is different...
			if (transitionBarrier.Subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES &&
				!resourceState.m_SubresourceStates.empty())
			{
				// First transition all of the subresources if they are different than the StateAfter.
				for (const auto subresourceState : resourceState.m_SubresourceStates)
				{
					if (transitionBarrier.StateAfter != subresourceState.second)
					{
						D3D12_RESOURCE_BARRIER newBarrier = barrier;
						newBarrier.Transition.Subresource = subresourceState.first;
						newBarrier.Transition.StateBefore = subresourceState.second;
						m_ResourceBarriers.push_back(newBarrier);
					}
				}
			}
			else
			{
				const auto finalState = resourceState.GetSubResourceState(transitionBarrier.Subresource);
				if (transitionBarrier.StateAfter != finalState)
				{
					// Push a new transition barrier with the correct before state.
					D3D12_RESOURCE_BARRIER newBarrier = barrier;
					newBarrier.Transition.StateBefore = finalState;
					m_ResourceBarriers.push_back(newBarrier);
				}
			}
		}
		else // In this case, the resource is being used on the command list for the first time. 
		{
			// Add a pending barrier. The pending barriers will be resolved
			// before the command list is executed on the command queue.
			m_PendingResourceBarriers.push_back(barrier);
		}

		// Push the final known state (possibly replacing the previously known state for the subresource).
		m_FinalResourceStates[transitionBarrier.pResource].SetSubresourceState(
			transitionBarrier.Subresource, transitionBarrier.StateAfter);
	}
	else
	{
		// Just push non-transition barriers to the resource barriers array.
		m_ResourceBarriers.push_back(barrier);
	}
}

void ResourceStateTracker::TransitionResource(ID3D12Resource* resource, const D3D12_RESOURCE_STATES stateAfter,
	const UINT subResource)
{
	if (resource)
	{
		ResourceBarrier(
			CD3DX12_RESOURCE_BARRIER::Transition(resource, D3D12_RESOURCE_STATE_COMMON, stateAfter, subResource));
	}
}

void ResourceStateTracker::TransitionResource(const Resource& resource, const D3D12_RESOURCE_STATES stateAfter,
	const UINT subResource)
{
	TransitionResource(resource.GetD3D12Resource().Get(), stateAfter, subResource);
}


void ResourceStateTracker::UavBarrier(const Resource* resource)
{
	ID3D12Resource* pResource = resource != nullptr ? resource->GetD3D12Resource().Get() : nullptr;
	ResourceBarrier(CD3DX12_RESOURCE_BARRIER::UAV(pResource));
}

void ResourceStateTracker::AliasBarrier(const Resource* beforeResource, const Resource* afterResource)
{
	ID3D12Resource* pResourceBefore = beforeResource != nullptr ? beforeResource->GetD3D12Resource().Get() : nullptr;
	ID3D12Resource* pResourceAfter = afterResource != nullptr ? afterResource->GetD3D12Resource().Get() : nullptr;
	ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Aliasing(pResourceBefore, pResourceAfter));
}

void ResourceStateTracker::FlushResourceBarriers(const CommandList& commandList)
{
	const UINT numBarriers = static_cast<UINT>(m_ResourceBarriers.size());
	if (numBarriers == 0)
	{
		return;
	}

	const auto d3d12CommandList = commandList.GetGraphicsCommandList();
	d3d12CommandList->ResourceBarrier(numBarriers, m_ResourceBarriers.data());
	m_ResourceBarriers.clear();
}

uint32_t ResourceStateTracker::FlushPendingResourceBarriers(const CommandList& commandList)
{
	assert(s_IsLocked);

	// Resolve the pending resource barriers by checking the global state of the 
	// (sub)resources. Add barriers if the pending state and the global state do not match.
	ResourceBarriersType resourceBarriers;
	// Reserve enough space (worst-cast, all pending barriers).
	resourceBarriers.reserve(m_PendingResourceBarriers.size());

	for (auto pendingBarrier : m_PendingResourceBarriers)
	{
		// Only transition barriers should be pending...
		if (pendingBarrier.Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION)
		{
			auto pendingTransition = pendingBarrier.Transition;
			const auto& iter = s_GlobalResourceStates.find(pendingTransition.pResource);
			if (iter != s_GlobalResourceStates.end())
			{
				// If all subresources are being transitioned, and there are multiple
				// subresources of the resource that are in a different state...

				auto& resourceState = iter->second;
				if (pendingTransition.Subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES &&
					!resourceState.m_SubresourceStates.empty()
					)
				{
					// Transition all subresources
					for (const auto subresourceState : resourceState.m_SubresourceStates)
					{
						if (pendingTransition.StateAfter != subresourceState.second)
						{
							D3D12_RESOURCE_BARRIER newBarrier = pendingBarrier;
							newBarrier.Transition.Subresource = subresourceState.first;
							newBarrier.Transition.StateBefore = subresourceState.second;
							resourceBarriers.push_back(newBarrier);
						}
					}
				}
				else
				{
					// No (sub)resources need to be transitioned. Just add a single transition barrier (if needed).
					const auto globalState = (iter->second).GetSubResourceState(pendingTransition.Subresource);
					if (pendingTransition.StateAfter != globalState)
					{
						// Fix-up the before state based on current global state of the resource.
						pendingBarrier.Transition.StateBefore = globalState;
						resourceBarriers.push_back(pendingBarrier);
					}
				}
			}
		}
	}

	UINT numBarriers = static_cast<UINT>(resourceBarriers.size());
	if (numBarriers > 0)
	{
		auto d3d12CommandList = commandList.GetGraphicsCommandList();
		d3d12CommandList->ResourceBarrier(numBarriers, resourceBarriers.data());
	}

	m_PendingResourceBarriers.clear();
	return numBarriers;
}

void ResourceStateTracker::CommitFinalResourceStates()
{
	assert(s_IsLocked);

	// Commit final resource states to the global resource state array (map).
	for (const auto& resourceState : m_FinalResourceStates)
	{
		s_GlobalResourceStates[resourceState.first] = resourceState.second;
	}

	m_FinalResourceStates.clear();
}

void ResourceStateTracker::Reset()
{
	m_PendingResourceBarriers.clear();
	m_ResourceBarriers.clear();
	m_FinalResourceStates.clear();
}

void ResourceStateTracker::Lock()
{
	s_GlobalMutex.lock();
	s_IsLocked = true;
}

void ResourceStateTracker::Unlock()
{
	s_GlobalMutex.unlock();
	s_IsLocked = false;
}

void ResourceStateTracker::AddGlobalResourceState(ID3D12Resource* resource, const D3D12_RESOURCE_STATES state)
{
	if (resource != nullptr)
	{
		std::lock_guard<std::mutex> lock(s_GlobalMutex);
		s_GlobalResourceStates[resource].SetSubresourceState(D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, state);
	}
}

void ResourceStateTracker::RemoveGlobalResourceState(ID3D12Resource* resource)
{
	if (resource != nullptr)
	{
		std::lock_guard<std::mutex> lock(s_GlobalMutex);
		s_GlobalResourceStates.erase(resource);
	}
}
