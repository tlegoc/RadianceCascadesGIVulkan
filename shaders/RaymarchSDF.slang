[[vk::binding(0)]]
Sampler2D SDFTexture : register(t0): register(s0);
RWTexture2D<float4> cascadeTexture;

struct PushConstants {
    uint32_t maxLevel;
    uint32_t verticalProbeCountAtMaxLevel; // not used
    float radius;
    float radiusMultiplier;
    float raymarchStepSize;
    float attenuation;
    uint32_t currentLevel;
}

#include "Common.slangi"

[shader("compute")]
[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID, uniform PushConstants pc)
{
    TextureInfo cascadeTextureInfo = GetTextureInfo(cascadeTexture);

    TextureInfo SDFTextureInfo = GetTextureInfo(SDFTexture);

    if (id.x >= cascadeTextureInfo.width || id.y >= cascadeTextureInfo.height) return;
    
    CascadeInfo cascadeInfo = GetCascadeInfo(pc.currentLevel, cascadeTextureInfo);

    Ray ray = cascadeInfo.GetRay(id, pc.radius, pc.radiusMultiplier);

    ray = RayCorrection(ray, cascadeTextureInfo, SDFTextureInfo);

    cascadeTexture[id.xy] = Raymarch(ray, pc.raymarchStepSize, pc.attenuation, SDFTextureInfo);
}
