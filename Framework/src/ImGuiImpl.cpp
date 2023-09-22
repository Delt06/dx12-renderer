#include "ImGuiImpl.h"

#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>

#include <DX12Library/Application.h>
#include <DX12Library/Helpers.h>

#include <Framework/Blit_VS.h>
#include <Framework/ImGuiCombine_PS.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

ImGuiImpl::ImGuiImpl(CommandList& commandList, const Window& window, const std::shared_ptr<CommonRootSignature>& pRootSignature)
    : m_CombineShader(std::make_shared<Shader>(pRootSignature,
        ShaderBlob(ShaderBytecode_Blit_VS, sizeof ShaderBytecode_Blit_VS),
        ShaderBlob(ShaderBytecode_ImGuiCombine_PS, sizeof ShaderBytecode_ImGuiCombine_PS),
        [](PipelineStateBuilder& psb)
        {
            psb.WithAlphaBlend();
        }
    ))
    , m_BlitMesh(Mesh::CreateBlitTriangle(commandList))
    , m_RootSignature(pRootSignature)
{
    const auto pDevice = Application::Get().GetDevice();

    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.NumDescriptors = 1;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        ThrowIfFailed(pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_SrvDescHeap)));
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    (void) io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(window.GetWindowHandle());
    ImGui_ImplDX12_Init(
        pDevice.Get(),
        Window::BUFFER_COUNT,
        BUFFER_FORMAT,
        m_SrvDescHeap.Get(),
        m_SrvDescHeap->GetCPUDescriptorHandleForHeapStart(),
        m_SrvDescHeap->GetGPUDescriptorHandleForHeapStart()
    );

    Application::AddWndProcHandler(ImGui_ImplWin32_WndProcHandler);
}
ImGuiImpl::~ImGuiImpl()
{
    Application::RemoveWndProcHandler(ImGui_ImplWin32_WndProcHandler);
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

bool ImGuiImpl::WantsToCaptureMouse() const
{
    return ImGui::GetIO().WantCaptureMouse;
}

bool ImGuiImpl::WantsToCaptureKeyboard() const
{
    return ImGui::GetIO().WantCaptureKeyboard;
}

void ImGuiImpl::BeginFrame() const
{
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void ImGuiImpl::Render() const
{
    ImGui::Render();
}

void ImGuiImpl::DrawToRenderTarget(CommandList& commandList)
{
    commandList.CommitStagedDescriptors();
    commandList.FlushResourceBarriers();

    const auto pD3dCmd = commandList.GetGraphicsCommandList();
    pD3dCmd->SetDescriptorHeaps(1, m_SrvDescHeap.GetAddressOf());
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), pD3dCmd.Get());
}

void ImGuiImpl::BlitCombine(CommandList& commandList, const std::shared_ptr<Texture>& pSourceTexture) const
{
    m_CombineShader->Bind(commandList);
    m_RootSignature->SetMaterialShaderResourceView(commandList, 0, ShaderResourceView(pSourceTexture));
    m_BlitMesh->Draw(commandList);
}
