#define THREAD_GROUP_SIZE_X 0 
#define THREAD_GROUP_SIZE_Y 0
#define THREAD_GROUP_SIZE_Z 0

struct FModelSdfData
{
	float4x4 LocalMatrix;
	float4x4 WorldMatrix;
    float4x4 CoordMatrix;
	
    float3 SdfLower;
    float3 SdfUpper;
};

#if defined(THREAD_GROUP_SIZE_X) && defined(THREAD_GROUP_SIZE_Y) && defined(THREAD_GROUP_SIZE_Z)
    
cbuffer gPassConstant : register(b0)
{
    float4x4 VoxelWorldMatrix;

    uint3 VoxelOffset;  float fGIMaxDistance;
    uint dwMeshSdfCount;   float3 PAD;
};


StructuredBuffer<FModelSdfData> gModelSdfDatas : register(t0);
Texture3D<float> gMeshSdfTextures[] : register(t1);
SamplerState gSampler : register(s0);

RWTexture3D<float> gGlobalSdfTexture : register(u0);

float CalcSdf(float fMinSdf, uint dwSdfIndex, float3 VoxelWorldPos);


[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y, THREAD_GROUP_SIZE_Z)]
void CS(uint3 ThreadID : SV_DispatchThreadID)
{
    uint3 VoxelID = VoxelOffset + ThreadID;
    float3 VoxelWorldPos = mul(float4(VoxelID, 1.0f), VoxelWorldMatrix);

    float fMinSdf = fGIMaxDistance;

    for (uint ix = 0; ix < dwMeshSdfCount; ++ix)
    {
        fMinSdf = CalcSdf(fMinSdf, ix, VoxelWorldPos);
    }

    gGlobalSdfTexture[VoxelID] = fMinSdf;
}


float CalcSdf(float fMinSdf, uint dwSdfIndex, float3 VoxelWorldPos)
{
    FModelSdfData SdfData = gModelSdfDatas[dwSdfIndex];

    float3 SdfLocalPos = mul(float4(VoxelWorldPos, 1.0f), SdfData.LocalMatrix).xyz;
    float3 SdfLocalPosClamped = clamp(SdfLocalPos, SdfData.SdfLower, SdfData.SdfUpper);
    float3 WorldPosClamped = mul(float4(SdfLocalPosClamped, 1.0f), SdfData.WorldMatrix).xyz;

    float fDistanceToSdf = length(VoxelWorldPos - WorldPosClamped);
    // 到 sdf 包围盒的距离已经大于当前最小距离.
    if (fMinSdf <= fDistanceToSdf) return fMinSdf;

    // float3 uvw = mul(float4(SdfLocalPosClamped, 1.0f), SdfData.CoordMatrix);

    float3 SdfExtent = SdfData.SdfUpper - SdfData.SdfLower;
    float u = (SdfLocalPosClamped.x - SdfData.SdfLower.x) / SdfExtent.x;
    float v = (SdfData.SdfUpper.y - SdfLocalPosClamped.y) / SdfExtent.y;
    float w = (SdfLocalPosClamped.z - SdfData.SdfLower.z) / SdfExtent.z;
    float3 uvw = float3(u, v, w);

    float sdf = gMeshSdfTextures[dwSdfIndex].SampleLevel(gSampler, uvw, 0);
    // Voxel 在 MeshSdf 内.
    if (fDistanceToSdf < 0.001f) return min(sdf, fMinSdf);

    // 精度非常低.
    return min(fMinSdf, sqrt(sdf * sdf + fDistanceToSdf *fDistanceToSdf));
}


#endif