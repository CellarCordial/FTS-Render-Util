#include "../Material.hlsli"
#include "../Octohedral.hlsli"

cbuffer FPassConstants : register(b0)
{
    float4x4 ViewProj;
    float4x4 PrevViewProj;
};

cbuffer FGeometryConstants : register(b1)
{
    float4x4 WorldMatrix;
    float4x4 InvTransWorld;

    uint dwMaterialIndex;
    uint3 Pad;
};


SamplerState gSampler : register(s0);

Texture2D gDiffuse           : register(t0);
Texture2D gNormal            : register(t1);
Texture2D gEmissive          : register(t2);
Texture2D gOcclusion         : register(t3);
Texture2D gMetallicRoughness : register(t4);

StructuredBuffer<FMaterial> gMaterials : register(t5);


struct FVertexInput
{
    float3 PositionL : POSITION;
    float3 NormalL   : NORMAL;
    float4 TangentL  : TANGENT;
    float2 UV        : TEXCOORD;
};

struct FVertexOutput
{
    float4 PositionH : SV_Position;
    float4 ClipPos      : CLIP_POSITION;
    float4 PrevClipPos  : PREV_CLIP_POSITION;
    float3 PositionW : POSITION;
    float3 NormalW   : NORMAL;
    float4 TangentW  : TANGENT;
    float2 UV        : TEXCOORD;
};


FVertexOutput VS(FVertexInput In)
{
    FVertexOutput Out;
    float4 CurrPosW = mul(float4(In.PositionL, 1.0f), WorldMatrix);

    Out.PositionH = mul(CurrPosW, ViewProj);

    Out.ClipPos = Out.PositionH;
    Out.PrevClipPos = mul(CurrPosW, PrevViewProj);

    Out.PositionW = CurrPosW.xyz;
    Out.NormalW = normalize(mul(float4(In.NormalL, 1.0f), InvTransWorld)).xyz;
    Out.TangentW = normalize(mul(In.TangentL, InvTransWorld));
    Out.UV = In.UV;
    return Out;
}


struct FPixelOutput
{
    float4 Position     : SV_Target0;
    float2 Normal       : SV_Target1;
    float4 BaseColor    : SV_Target2;
    float4 PBR          : SV_Target3;
    float4 Emmisive     : SV_Target4;
};

float3 CalcNormal(float3 TextureNormal, float3 VertexNormal, float4 VertexTangent)
{
    float3 UnpackedNormal = TextureNormal * 2.0f - 1.0f;
    float3 N = VertexNormal;
    float3 T = normalize(VertexTangent.xyz - N * dot(VertexTangent.xyz, N));
    float3 B = cross(N, T) * VertexTangent.w;
    float3x3 TBN = float3x3(T, B, N);
    return normalize(mul(UnpackedNormal, TBN));
}

FPixelOutput PS(FVertexOutput In)
{
    FPixelOutput Out;
    Out.Position = float4(In.PositionW, 1.0f);
    
    float3 Normal = CalcNormal(gNormal.Sample(gSampler, In.UV).xyz, In.NormalW, In.TangentW);
    Out.Normal = UnitVectorToOctahedron(Normal);

    Out.BaseColor = gDiffuse.Sample(gSampler, In.UV) * gMaterials[dwMaterialIndex].fDiffuse;
    
    float fOcclusion = gOcclusion.Sample(gSampler, In.UV).r;
    float4 MetallicRoughness = gMetallicRoughness.Sample(gSampler, In.UV);
    MetallicRoughness.r *= gMaterials[dwMaterialIndex].fMetallic;
    MetallicRoughness.g *= gMaterials[dwMaterialIndex].fRoughness;
    Out.PBR = float4(MetallicRoughness.rg, fOcclusion, 1.0f);

    Out.Emmisive = gEmissive.Sample(gSampler, In.UV);
    return Out;
}