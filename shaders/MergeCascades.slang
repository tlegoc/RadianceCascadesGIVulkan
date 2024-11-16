RWTexture2D<float4> baseCascadeTexture;
RWTexture2D<float4> outputCascadeTexture;

struct PushConstants {
    uint32_t maxLevel;
    uint32_t verticalProbeCountAtMaxLevel;
    float radius;
    float radiusMultiplier;
    float raymarchStepSize;
    float attenuation;
    uint32_t outputLevel;
};

#include "Common.slangi"

[shader("compute")]
[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID, uniform PushConstants pc)
{
    TextureInfo baseCascadeTextureInfo = GetTextureInfo(baseCascadeTexture);
    TextureInfo outputCascadeTextureInfo = GetTextureInfo(outputCascadeTexture);

    if (id.x >= baseCascadeTextureInfo.width || id.y >= baseCascadeTextureInfo.height) return;
    
    CascadeInfo baseCascadeInfo = GetCascadeInfo(pc.outputLevel - 1, baseCascadeTextureInfo);
    CascadeInfo outputCascadeInfo = GetCascadeInfo(pc.outputLevel, outputCascadeTextureInfo);

    Ray ray = outputCascadeInfo.GetRay(id, pc.radius, pc.radiusMultiplier);

    float normalizedAngle = ray.angle / (2 * 3.141592653589793);
    int angleIndex = floor(normalizedAngle * baseCascadeInfo.probeRayCount);
    int rays[4] = {
        angleIndex - 1,
        angleIndex,
        angleIndex + 1,
        angleIndex + 2
    };
}
