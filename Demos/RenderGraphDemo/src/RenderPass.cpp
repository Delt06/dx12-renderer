#include "RenderPass.h"

void RenderGraph::RenderPass::Init()
{
    InitImpl();
}

void RenderGraph::RenderPass::Execute(CommandList& commandList)
{
    ExecuteImpl(commandList);
}

void RenderGraph::RenderPass::RegisterInput(RenderGraph::RenderPass::Input input)
{
    Assert(std::find_if(m_Inputs.begin(), m_Inputs.end(), [input](const auto& i) { return i.m_Id == input.m_Id; }) == m_Inputs.end(), "Input with such ID is already registered.");

    m_Inputs.push_back(input);
}

void RenderGraph::RenderPass::RegisterOutput(RenderGraph::RenderPass::Output output)
{
    Assert(output.m_Type != OutputType::Invalid, "Output is invalid.");
    Assert(std::find_if(m_Outputs.begin(), m_Outputs.end(), [output](const auto& o) { return o.m_Id == output.m_Id; }) == m_Outputs.end(), "Output with such ID is already registered.");

    m_Outputs.push_back(output);
}

void RenderGraph::RenderPass::SetPassName(const wchar_t* passName)
{
    m_PassName = passName;
}

