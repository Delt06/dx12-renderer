#pragma once

#include <memory>
#include <string>
#include <vector>

#include <DX12Library/CommandList.h>
#include <DX12Library/Helpers.h>

#include "RenderContext.h"
#include "ResourceId.h"

namespace RenderGraph
{
    class RenderPass
    {
    public:
        enum class InputType
        {
            Invalid,
            ShaderResource,
            CopySource,
        };

        struct Input
        {
            ResourceId m_Id = 0;
            InputType m_Type = InputType::Invalid;
        };

        enum class OutputType
        {
            Invalid,
            RenderTarget,
            DepthRead,
            DepthWrite,
            UnorderedAccess,
            CopyDestination,
        };

        enum OutputInitAction
        {
            None,
            Clear,
        };

        struct Output
        {
            ResourceId m_Id = 0;
            OutputType m_Type = OutputType::Invalid;
            OutputInitAction m_InitAction = OutputInitAction::None;
        };

        void Init(CommandList& commandList);

        void Execute(const RenderContext& context, CommandList& commandList);

        const std::vector<Input>& GetInputs() const { return m_Inputs; }
        const std::vector<Output>& GetOutputs() const { return m_Outputs; }
        const std::wstring& GetPassName() const {return m_PassName;}

    protected:
        virtual void InitImpl(CommandList& commandList) = 0;
        virtual void ExecuteImpl(const RenderContext& context, CommandList& commandList) = 0;

        void RegisterInput(Input input);
        void RegisterOutput(Output output);

        void SetPassName(const wchar_t* passName);
        void SetPassName(const std::wstring& passName);

    private:
        std::vector<Input> m_Inputs;
        std::vector<Output> m_Outputs;
        std::wstring m_PassName = L"Render Pass";
    };
}
