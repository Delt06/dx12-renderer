#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <DX12Library/CommandList.h>
#include <DX12Library/Helpers.h>

#include "RenderContext.h"
#include "ResourceId.h"

namespace RenderGraph
{
    enum class InputType
    {
        Invalid,
        Token,
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
        Token,
        RenderTarget,
        DepthRead,
        DepthWrite,
        UnorderedAccess,
        CopyDestination,
    };

    struct Output
    {
        ResourceId m_Id = 0;
        OutputType m_Type = OutputType::Invalid;
    };

    class RenderPass
    {
    public:
        using ExecuteFuncT = std::function<void(const RenderContext&, CommandList&)>;

        static std::unique_ptr<RenderPass> Create(
            const wchar_t* passName,
            const std::vector<Input>& inputs,
            const std::vector<Output>& outputs,
            const ExecuteFuncT& executeFunc
        );

        void Init(CommandList& commandList);

        void Execute(const RenderContext& context, CommandList& commandList);

        const std::vector<Input>& GetInputs() const { return m_Inputs; }
        const std::vector<Output>& GetOutputs() const { return m_Outputs; }
        const std::wstring& GetPassName() const { return m_PassName; }

    protected:
        virtual void InitImpl(CommandList& commandList) = 0;
        virtual void ExecuteImpl(const RenderContext& context, CommandList& commandList) = 0;

        void RegisterInput(const Input& input);
        void RegisterOutput(const Output& output);

        void SetPassName(const wchar_t* passName);
        void SetPassName(const std::wstring& passName);

    private:
        std::vector<Input> m_Inputs;
        std::vector<Output> m_Outputs;
        std::wstring m_PassName = L"Render Pass";
    };
}
