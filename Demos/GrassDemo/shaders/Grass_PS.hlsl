cbuffer Material : register(b0)
{
    float4 _Color;
};

float4 main() : SV_TARGET
{
    return _Color;
}
