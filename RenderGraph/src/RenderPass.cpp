#include "RenderPass.h"

namespace RenderGraph
{
    class LambdaRenderPass : public RenderPass
    {
    public:
        LambdaRenderPass(
            const std::vector<Input>& inputs,
            const std::vector<Output>& outputs,
            const ExecuteFuncT& executeFunc)
            : m_ExecuteFunc(executeFunc)
        {
            for (auto& input : inputs)
            {
                RegisterInput(input);
            }

            for (auto& output : outputs)
            {
                RegisterOutput(output);
            }
        }

    protected:
        virtual void InitImpl(CommandList& commandList) override
        {
        }

        virtual void ExecuteImpl(const RenderContext& context, CommandList& commandList) override
        {

            m_ExecuteFunc(context, commandList);
        }

    private:
        ExecuteFuncT m_ExecuteFunc;
    };
}

std::unique_ptr<RenderGraph::RenderPass> RenderGraph::RenderPass::Create(
    const wchar_t* passName,
    const std::vector<Input>& inputs,
    const std::vector<Output>& outputs,
    const ExecuteFuncT& executeFunc)
{
    auto pRenderPass = new LambdaRenderPass(inputs, outputs, executeFunc);
    pRenderPass->SetPassName(passName);
    return std::unique_ptr<RenderPass>(pRenderPass);
}

void RenderGraph::RenderPass::Init(CommandList& commandList)
{
    InitImpl(commandList);
}

void RenderGraph::RenderPass::Execute(const RenderGraph::RenderContext& context, CommandList& commandList)
{
    ExecuteImpl(context, commandList);
}

void RenderGraph::RenderPass::RegisterInput(const RenderGraph::Input& input)
{
    Assert(input.m_Type != InputType::Invalid, "Input is invalid.");
    Assert(std::find_if(m_Inputs.begin(), m_Inputs.end(), [input](const auto& i) { return i.m_Id == input.m_Id; }) == m_Inputs.end(), "Input with such ID is already registered.");

    m_Inputs.push_back(input);
}

void RenderGraph::RenderPass::RegisterOutput(const RenderGraph::Output& output)
{
    Assert(output.m_Type != OutputType::Invalid, "Output is invalid.");
    Assert(std::find_if(m_Outputs.begin(), m_Outputs.end(), [output](const auto& o) { return o.m_Id == output.m_Id; }) == m_Outputs.end(), "Output with such ID is already registered.");

    m_Outputs.push_back(output);
}

void RenderGraph::RenderPass::SetPassName(const wchar_t* passName)
{
    m_PassName = passName;
}

void RenderGraph::RenderPass::SetPassName(const std::wstring& passName)
{
    m_PassName = passName;
}

