struct VertexShaderInput
{
    float2 UV : TEXCOORD;
};

struct Parameters
{
    float WhitePoint;
};

Texture2D source : register(t0);
Texture2D averageLuminanceMap : register(t1);
SamplerState sourceSampler : register(s0);
ConstantBuffer<Parameters> parametersCB : register(b0);

float3 ConvertRGB2XYZ(float3 _rgb)
{
	// Reference:
	// RGB/XYZ Matrices
	// http://www.brucelindbloom.com/index.html?Eqn_RGB_XYZ_Matrix.html
    float3 xyz;
    xyz.x = dot(float3(0.4124564, 0.3575761, 0.1804375), _rgb);
    xyz.y = dot(float3(0.2126729, 0.7151522, 0.0721750), _rgb);
    xyz.z = dot(float3(0.0193339, 0.1191920, 0.9503041), _rgb);
    return xyz;
}

float3 ConvertXYZ2Yxy(float3 _xyz)
{
	// Reference:
	// http://www.brucelindbloom.com/index.html?Eqn_XYZ_to_xyY.html
    float inv = 1.0 / dot(_xyz, float3(1.0, 1.0, 1.0));
    return float3(_xyz.y, _xyz.x * inv, _xyz.y * inv);
}

float3 ConvertRGB2Yxy(float3 _rgb)
{
    return ConvertXYZ2Yxy(ConvertRGB2XYZ(_rgb));
}

float3 ConvertXYZ2RGB(float3 _xyz)
{
    float3 rgb;
    rgb.x = dot(float3(3.2404542, -1.5371385, -0.4985314), _xyz);
    rgb.y = dot(float3(-0.9692660, 1.8760108, 0.0415560), _xyz);
    rgb.z = dot(float3(0.0556434, -0.2040259, 1.0572252), _xyz);
    return rgb;
}

float3 ConvertYxy2XYZ(float3 _Yxy)
{
	// Reference:
	// http://www.brucelindbloom.com/index.html?Eqn_xyY_to_XYZ.html
    float3 xyz;
    xyz.x = _Yxy.x * _Yxy.y / _Yxy.z;
    xyz.y = _Yxy.x;
    xyz.z = _Yxy.x * (1.0 - _Yxy.y - _Yxy.z) / _Yxy.z;
    return xyz;
}

float3 ConvertYxy2RGB(float3 _Yxy)
{
    return ConvertXYZ2RGB(ConvertYxy2XYZ(_Yxy));
}

float Reinhard2(float _x, float _whiteSqr)
{
    return (_x * (1.0 + _x / _whiteSqr)) / (1.0 + _x);
}

float4 main(VertexShaderInput IN) : SV_TARGET
{
    float3 hdrColor = source.Sample(sourceSampler, IN.UV).rgb;
    float averageLuminance = averageLuminanceMap.Sample(sourceSampler, 0).r;
    
    // https://bruop.github.io/tonemapping/
    float3 Yxy = ConvertRGB2Yxy(hdrColor);
    float lp = Yxy.x / (9.6 * averageLuminance + 0.0001);
    Yxy.x = Reinhard2(lp, parametersCB.WhitePoint);
    float3 mapped = ConvertYxy2RGB(Yxy);
    
    // no need for gamma correction is using an SRGB render target
    return float4(mapped, 1.0);
}