#pragma once

#include <memory>
#include <string>
#include <vector>

#include <DX12Library/CommandList.h>
#include <DX12Library/Helpers.h>

#include "ResourceId.h"

namespace RenderGraph
{
    class RenderPass
    {
    public:
        struct Input
        {
            ResourceId m_Id = 0;
        };

        enum class OutputType
        {
            None,
            RenderTarget,
            DepthRead,
            DepthWrite,
            UnorderedAccess,
        };

        struct Output
        {
            ResourceId m_Id = 0;
            OutputType m_Type = OutputType::None;
        };

        void Init();

        void Execute(CommandList& commandList);

        const std::vector<Input>& GetInputs() const { return m_Inputs; }
        const std::vector<Output>& GetOutputs() const { return m_Outputs; }
        const std::wstring& GetPassName() const {return m_PassName;}

    protected:
        virtual void InitImpl() = 0;
        virtual void ExecuteImpl(CommandList& commandList) = 0;

        void RegisterInput(Input input);
        void RegisterOutput(Output output);

        void SetPassName(const wchar_t* passName);

    private:
        std::vector<Input> m_Inputs;
        std::vector<Output> m_Outputs;
        std::wstring m_PassName = L"Render Pass";
    };
}
