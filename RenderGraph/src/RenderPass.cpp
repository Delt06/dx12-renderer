#include "RenderPass.h"

namespace RenderGraph
{
    class LambdaRenderPass final : public RenderPass
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

        ~LambdaRenderPass() override = default;

    protected:
        void InitImpl(CommandList& commandList) override
        {}

        void ExecuteImpl(const RenderContext& context, CommandList& commandList) override
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
    const auto pRenderPass = new LambdaRenderPass(inputs, outputs, executeFunc);
    pRenderPass->SetPassName(passName);
    return std::unique_ptr<RenderPass>(pRenderPass);
}

void RenderGraph::RenderPass::Init(CommandList& commandList)
{
    InitImpl(commandList);
}

void RenderGraph::RenderPass::Execute(const RenderContext& context, CommandList& commandList)
{
    ExecuteImpl(context, commandList);
}

void RenderGraph::RenderPass::RegisterInput(const Input& input)
{
    Assert(input.m_Type != InputType::Invalid, "Input is invalid.");
    Assert(std::ranges::find_if(m_Inputs, [input](const auto& i) { return i.m_Id == input.m_Id; }) == m_Inputs.end(), "Input with such ID is already registered.");

    m_Inputs.push_back(input);
}

void RenderGraph::RenderPass::RegisterOutput(const Output& output)
{
    Assert(output.m_Type != OutputType::Invalid, "Output is invalid.");
    Assert(std::ranges::find_if(m_Outputs, [output](const auto& o) { return o.m_Id == output.m_Id; }) == m_Outputs.end(), "Output with such ID is already registered.");

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
