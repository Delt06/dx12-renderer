#define THREAD_BLOCK_SIZE 32

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
    uint flags;
    D3D12_DRAW_ARGUMENTS drawArguments;
};

AppendStructuredBuffer<IndirectCommand> _IndirectCommands : register(u0);

cbuffer CBuffer : register(b0)
{
    float3 _CameraPosition;
    uint _TotalCount;
}

bool ConeCulling(const in Meshlet meshlet)
{
    const Transform transform = _TransformsBuffer[meshlet.transformIndex];

    const float3 coneApex = mul(transform.worldMatrix, float4(meshlet.bounds.coneApex, 1.0f)).xyz;
    const float3 coneAxis = mul((float3x3) transform.inverseTransposeWorldMatrix, meshlet.bounds.coneAxis);

    const float dotResult = dot(normalize(coneApex - _CameraPosition), normalize(coneAxis));
    // using !>= handles the case when the meshlet's coneAxis is (0, 0, 0)
    // dotResult stores NaN in this case
    return !(dotResult >= meshlet.bounds.coneCutoff);
}

bool Culling(const in Meshlet meshlet)
{
    return ConeCulling(meshlet);
}

[numthreads(THREAD_BLOCK_SIZE, 1, 1)]
void main(in uint3 dispatchId : SV_DispatchThreadID)
{
    const uint meshletIndex = dispatchId.x;
    if (meshletIndex >= _TotalCount)
    {
        return;
    }

    const Meshlet meshlet = _MeshletsBuffer[meshletIndex];

    // if (Culling(meshlet))
    {
        IndirectCommand indirectCommand;
        indirectCommand.meshletIndex = meshletIndex;
        indirectCommand.drawArguments.VertexCountPerInstance = meshlet.indexCount;
        indirectCommand.drawArguments.InstanceCount = 1;
        indirectCommand.drawArguments.StartVertexLocation = 0;
        indirectCommand.drawArguments.StartInstanceLocation = 0;
        indirectCommand.flags = 0;

        if (ConeCulling(meshlet))
        {
            indirectCommand.flags |= MESHLET_FLAGS_PASSED_CONE_CULLING;
        }

        _IndirectCommands.Append(indirectCommand);
    }
}
