struct MaterialCb
{
	float4 Color;
};

ConstantBuffer<MaterialCb> materialCb : register(b0, space1);

float4 main() : SV_TARGET
{
	return materialCb.Color;
}
