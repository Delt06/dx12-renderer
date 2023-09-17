#define THREAD_BLOCK_SIZE 32

#include <ShaderLibrary/Common/RootSignature.hlsli>
#include "ShaderLibrary/Meshlets.hlsli"

struct D3D12_DRAW_ARGUMENTS
{
    uint VertexCountPerInstance;
    uint InstanceCount;
    uint StartVertexLocation;
    uint StartInstanceLocation;
};

struct IndirectCommand
{
    uint meshletIndex;
    D3D12_DRAW_ARGUMENTS drawArguments;
    uint _pad;
};

AppendStructuredBuffer<IndirectCommand> _IndirectCommands : register(u0);

cbuffer CBuffer : register(b0, COMMON_ROOT_SIGNATURE_CONSTANTS_SPACE)
{
    uint _TotalCount;
};

[numthreads(THREAD_BLOCK_SIZE, 1, 1)]
void main(in uint3 dispatchId : SV_DispatchThreadID)
{
    const uint meshletIndex = dispatchId.x;
    if (meshletIndex >= _TotalCount)
    {
        return;
    }

    const Meshlet meshlet = _MeshletsBuffer[meshletIndex];

    IndirectCommand indirectCommand = (IndirectCommand) 0;
    indirectCommand.meshletIndex = meshletIndex;
    indirectCommand.drawArguments.VertexCountPerInstance = meshlet.indexCount;
    indirectCommand.drawArguments.InstanceCount = 1;
    indirectCommand.drawArguments.StartVertexLocation = 0;
    indirectCommand.drawArguments.StartInstanceLocation = 0;

    _IndirectCommands.Append(indirectCommand);
}
