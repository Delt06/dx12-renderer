#include "Tutorial2.h"
#include "../Application.h"
#include "../CommandQueue.h"
#include "../Helpers.h"
#include "../Window.h"

#include <wrl.h>
using namespace Microsoft::WRL;

#include <d3dx12.h>
#include <d3dcompiler.h>

#include <algorithm> // For std::min and std::max.
#if defined(min)
#undef min
#endif

#if defined(max)
#undef max
#endif

using namespace DirectX;

struct VertexAppData
{
	XMFLOAT3 Position;
	XMFLOAT3 Color;
};

static VertexAppData vertices[8] = {
	{XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f)}, // 0
	{XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f)}, // 1
	{XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT3(1.0f, 1.0f, 0.0f)}, // 2
	{XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f)}, // 3
	{XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f)}, // 4
	{XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 1.0f, 1.0f)}, // 5
	{XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(1.0f, 1.0f, 1.0f)}, // 6
	{XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT3(1.0f, 0.0f, 1.0f)} // 7
};

static WORD indices[36] =
{
	0, 1, 2, 0, 2, 3,
	4, 6, 5, 4, 7, 6,
	4, 5, 1, 4, 1, 0,
	3, 2, 6, 3, 6, 7,
	1, 5, 6, 1, 6, 2,
	4, 0, 3, 4, 3, 7
};

Tutorial2::Tutorial2(const std::wstring& name, const int width, const int height, const bool vSync) :
	Base(name, width, height, vSync),
	VertexBufferView(), IndexBufferView(),
	Viewport(CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height))),
	ScissorRect(CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX)),
	Fov(45.0f),
	ModelMatrix(), ViewMatrix(), ProjectionMatrix(),
	IsContentLoaded(false)
{
}

void Tutorial2::UpdateBufferResource(ComPtr<ID3D12GraphicsCommandList2> commandList,
                                     ID3D12Resource** pDestinationResource, ID3D12Resource** pIntermediateResource,
                                     const size_t numElements, const size_t elementSize,
                                     const void* bufferData,
                                     const D3D12_RESOURCE_FLAGS flags)
{
	const auto device = Application::Get().GetDevice();
	const size_t bufferSize = numElements * elementSize;

	{
		const auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		const auto bufferResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize, flags);
		// Create a committed resource for the GPU resource in a default heap.
		ThrowIfFailed(device->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&bufferResourceDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(pDestinationResource)
		));
	}

	// Create a committed resource for the upload
	if (bufferData)
	{
		const auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		const auto bufferResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
		ThrowIfFailed(device->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&bufferResourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(pIntermediateResource)
		));

		D3D12_SUBRESOURCE_DATA subresourceData = {};
		subresourceData.pData = bufferData;
		subresourceData.RowPitch = static_cast<LONG_PTR>(bufferSize);
		subresourceData.SlicePitch = subresourceData.RowPitch;

		UpdateSubresources(commandList.Get(),
		                   *pDestinationResource, *pIntermediateResource,
		                   0, 0, 1, &subresourceData
		);
	}
}

bool Tutorial2::LoadContent()
{
	const auto device = Application::Get().GetDevice();
	const auto commandQueue = Application::Get().GetCommandQueue();
	const auto commandList = commandQueue->GetCommandList();

	// Upload vertex buffer data
	ComPtr<ID3D12Resource> intermediateVertexBuffer;
	UpdateBufferResource(commandList.Get(),
	                     &VertexBuffer, &intermediateVertexBuffer,
	                     _countof(vertices), sizeof(VertexAppData),
	                     vertices);
	// Create the vertex buffer view
	VertexBufferView.BufferLocation = VertexBuffer->GetGPUVirtualAddress();
	VertexBufferView.SizeInBytes = sizeof vertices;
	VertexBufferView.StrideInBytes = sizeof(VertexAppData);

	// Upload index buffer data
	ComPtr<ID3D12Resource> intermediateIndexBuffer;
	UpdateBufferResource(commandList.Get(),
	                     &IndexBuffer, &intermediateIndexBuffer,
	                     _countof(indices), sizeof(WORD),
	                     indices
	);
	// Create index buffer view
	IndexBufferView.BufferLocation = IndexBuffer->GetGPUVirtualAddress();
	IndexBufferView.Format = DXGI_FORMAT_R16_UINT;
	IndexBufferView.SizeInBytes = sizeof indices;


	// Create the descriptor heap for the depth-stencil view
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	ThrowIfFailed(device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&DsvHeap)));

	// Load the vertex shader
	ComPtr<ID3DBlob> vertexShaderBlob;
	ThrowIfFailed(D3DReadFileToBlob(L"Tutorial2_VertexShader.cso", &vertexShaderBlob));

	// Load the pixel shader
	ComPtr<ID3DBlob> pixelShaderBlob;
	ThrowIfFailed(D3DReadFileToBlob(L"Tutorial2_PixelShader.cso", &pixelShaderBlob));

	// Create the vertex input layout
	D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
		{
			"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		},
		{
			"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		},
	};

	// Create a root signature
	D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
	featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
	if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
	{
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	}
	// Allow input layout and deny unnecessary access to certain pipeline stages
	constexpr D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

	// A single 32-bit constant root parameter that is usd by the vertex shader
	CD3DX12_ROOT_PARAMETER1 rootParameters[1];
	rootParameters[0].InitAsConstants(sizeof(XMMATRIX) / sizeof(float), 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
	rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr, rootSignatureFlags);

	// Serialize the root signature
	ComPtr<ID3DBlob> rootSignatureBlob;
	ComPtr<ID3DBlob> errorBlob;
	ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion,
	                                                    &rootSignatureBlob, &errorBlob));
	// Create the root signature
	ThrowIfFailed(device->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(),

	                                          rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&RootSignature)));

	// Create Pipeline State Stream
	struct PipelineStateStream
	{
		CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE RootSignature;
		CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
		CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
		CD3DX12_PIPELINE_STATE_STREAM_VS Vs;
		CD3DX12_PIPELINE_STATE_STREAM_PS Ps;
		CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DsvFormat;
		CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RtvFormats;
	} pipelineStateStream;

	CD3DX12_RT_FORMAT_ARRAY rtvFormats = {};
	rtvFormats.NumRenderTargets = 1;
	rtvFormats.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

	pipelineStateStream.RootSignature = RootSignature.Get();
	pipelineStateStream.InputLayout = {inputLayout, _countof(inputLayout)};
	pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pipelineStateStream.Vs = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
	pipelineStateStream.Ps = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
	pipelineStateStream.DsvFormat = DXGI_FORMAT_D32_FLOAT;
	pipelineStateStream.RtvFormats = rtvFormats;

	const D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc{
		sizeof(PipelineStateStream), &pipelineStateStream
	};
	ThrowIfFailed(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&PipelineState)));

	const auto fenceValue = commandQueue->ExecuteCommandList(commandList);
	commandQueue->WaitForFenceValue(fenceValue);

	IsContentLoaded = true;

	// Resize/Create the depth buffer
	ResizeDepthBuffer(GetClientWidth(), GetClientHeight());

	return true;
}

void Tutorial2::ResizeDepthBuffer(int width, int height)
{
	if (!IsContentLoaded)
	{
		return;
	}

	// Flush any GPU commands that might be referencing the depth buffer
	Application::Get().Flush();

	width = std::max(1, width);
	height = std::max(1, height);

	const auto device = Application::Get().GetDevice();

	// Resize screen dependent resources
	// Create a depth buffer
	CD3DX12_CLEAR_VALUE optimizedClearValue = {};
	optimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
	optimizedClearValue.DepthStencil = {1.0f, 0};

	const auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	const auto resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT,
	                                                       width, height, 1, 0, 1, 0,
	                                                       D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
	ThrowIfFailed(device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&optimizedClearValue,
		IID_PPV_ARGS(&DepthBuffer)
	));

	// Update the depth-stencil view
	D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {};
	dsv.Format = DXGI_FORMAT_D32_FLOAT;
	dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsv.Texture2D.MipSlice = 0;
	dsv.Flags = D3D12_DSV_FLAG_NONE;

	device->CreateDepthStencilView(DepthBuffer.Get(), &dsv, DsvHeap->GetCPUDescriptorHandleForHeapStart());
}

void Tutorial2::OnResize(ResizeEventArgs& e)
{
	if (e.Width == GetClientWidth() && e.Height == GetClientHeight())
	{
		return;
	}

	Base::OnResize(e);
	Viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(e.Width), static_cast<float>(e.Height));
	ResizeDepthBuffer(e.Width, e.Height);
}

void Tutorial2::OnUpdate(UpdateEventArgs& e)
{
	static uint64_t frameCount = 0;
	static double totalTime = 0.0;

	Base::OnUpdate(e);

	totalTime += e.ElapsedTime;
	frameCount++;

	if (totalTime > 1.0)
	{
		const double fps = static_cast<double>(frameCount) / totalTime;

		char buffer[512];
		sprintf_s(buffer, "FPS: %f\n", fps);
		OutputDebugStringA(buffer);

		frameCount = 0;
		totalTime = 0.0;
	}

	// Update the model matrix
	const auto angle = static_cast<float>(e.TotalTime * 90.0);
	const XMVECTOR rotationAxis = XMVectorSet(0, 1, 1, 0);
	ModelMatrix = XMMatrixRotationAxis(rotationAxis, XMConvertToRadians(angle));

	// Update the view matrix
	const XMVECTOR eyePosition = XMVectorSet(0, 0, -10, 1);
	const XMVECTOR focusPosition = XMVectorSet(0, 0, 0, 1);
	const XMVECTOR upDirection = XMVectorSet(0, 1, 0, 0);
	ViewMatrix = XMMatrixLookAtLH(eyePosition, focusPosition, upDirection);

	// Update the projection matrix
	const float aspectRatio = GetClientWidth() / static_cast<float>(GetClientHeight());
	ProjectionMatrix = XMMatrixPerspectiveFovLH(XMConvertToRadians(Fov), aspectRatio, 0.1f, 100.f);
}

void Tutorial2::TransitionResource(const ComPtr<ID3D12GraphicsCommandList2> commandList,
                                   const ComPtr<ID3D12Resource>& resource,
                                   const D3D12_RESOURCE_STATES beforeState, const D3D12_RESOURCE_STATES afterState)
{
	const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource.Get(), beforeState, afterState);
	commandList->ResourceBarrier(1, &barrier);
}

void Tutorial2::ClearRtv(const ComPtr<ID3D12GraphicsCommandList2> commandList, const D3D12_CPU_DESCRIPTOR_HANDLE rtv,
                         const FLOAT* clearColor)
{
	commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
}

void Tutorial2::ClearDepth(const ComPtr<ID3D12GraphicsCommandList2> commandList, const D3D12_CPU_DESCRIPTOR_HANDLE dsv,
                           const FLOAT depth)
{
	commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, depth, 0, 0, nullptr);
}

void Tutorial2::OnRender(RenderEventArgs& e)
{
	Base::OnRender(e);

	const auto commandQueue = Application::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	const auto commandList = commandQueue->GetCommandList();

	const UINT currentBackBufferIndex = PWindow->GetCurrentBackBufferIndex();
	const auto backBuffer = PWindow->GetCurrentBackBuffer();
	const auto rtv = PWindow->GetCurrentRenderTargetView();
	const auto dsv = DsvHeap->GetCPUDescriptorHandleForHeapStart();

	// Clear the render targets
	{
		TransitionResource(commandList, backBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

		constexpr FLOAT clearColor[] = {0.4f, 0.6f, 0.9f, 1.0f};

		ClearRtv(commandList, rtv, clearColor);
		ClearDepth(commandList, dsv);
	}

	// Set pipeline state and root signature
	{
		commandList->SetPipelineState(PipelineState.Get());
		commandList->SetGraphicsRootSignature(RootSignature.Get());
	}

	// Setup the input assembler
	{
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->IASetVertexBuffers(0, 1, &VertexBufferView);
		commandList->IASetIndexBuffer(&IndexBufferView);
	}

	// Setup the rasterizer state
	{
		commandList->RSSetViewports(1, &Viewport);
		commandList->RSSetScissorRects(1, &ScissorRect);
	}

	// Bind the render targets for the output merger
	{
		commandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
	}

	// Update the root parameters
	{
		// Update the MVP matrix
		XMMATRIX mvpMatrix = XMMatrixMultiply(ModelMatrix, ViewMatrix);
		mvpMatrix = XMMatrixMultiply(mvpMatrix, ProjectionMatrix);
		commandList->SetGraphicsRoot32BitConstants(0, sizeof(XMMATRIX) / sizeof(float), &mvpMatrix, 0);
	}

	// Draw!
	{
		commandList->DrawIndexedInstanced(_countof(indices), 1, 0, 0, 0);
	}

	// Present
	{
		TransitionResource(commandList, backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

		FenceValues[currentBackBufferIndex] = commandQueue->ExecuteCommandList(commandList);

		PWindow->Present();

		commandQueue->WaitForFenceValue(FenceValues[currentBackBufferIndex]);
	}
}

void Tutorial2::UnloadContent()
{
	// TODO: implement
}

void Tutorial2::OnKeyPressed(KeyEventArgs& e)
{
	Base::OnKeyPressed(e);

	switch (e.Key)
	{
	case KeyCode::Escape:
		Application::Get().Quit(0);
		break;
	case KeyCode::Enter:
		if (e.Alt)
		{
		case KeyCode::F11:
			PWindow->ToggleFullscreen();
			break;
		}
		break;
	case KeyCode::V:
		PWindow->ToggleVSync();
		break;
	default:
		break;
	}
}

// Clamp a value between a min and max range.
template <typename T>
static constexpr const T& Clamp(const T& val, const T& min, const T& max)
{
	return val < min ? min : val > max ? max : val;
}

void Tutorial2::OnMouseWheel(MouseWheelEventArgs& e)
{
	Fov -= e.WheelDelta;
	Fov = Clamp(Fov, 12.0f, 90.0f);

	char buffer[256];
	sprintf_s(buffer, "FOV: %f\n", Fov);
	OutputDebugStringA(buffer);
}
