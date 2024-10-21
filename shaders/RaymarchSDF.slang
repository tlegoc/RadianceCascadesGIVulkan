[[vk::binding(0)]]
Sampler2D inputSDF : register(t0): register(s0);
RWTexture2D<float4> outputCascade;

struct PushConstants {
    uint32_t maxLevel;
    uint32_t verticalProbeCountAtMaxLevel;
    float radius;
    float radiusMultiplier;
    float raymarchStepSize;
    float attenuation;
    uint32_t currentLevel;
}

[shader("compute")]
[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID, uniform PushConstants pc)
{
    // Texture data
    uint outputCascadeWidth, outputCascadeHeight, outputCascadelevels;
    outputCascade.GetDimensions(0,outputCascadeWidth, outputCascadeHeight, outputCascadelevels);
    float2 outputCascadeResolution = float2(outputCascadeWidth, outputCascadeHeight);
    float outputCascadeAspectRatio = outputCascadeResolution.x/outputCascadeResolution.y;

    uint inputWidth, inputHeight, inputLevels;
    inputSDF.GetDimensions(0, inputWidth, inputHeight, inputLevels);
    float2 inputSDFResolution = float2(inputWidth, inputHeight);
    float inputSDFAspectRatio = inputSDFResolution.x/inputSDFResolution.y;

    if (id.x >= outputCascadeWidth || id.y >= outputCascadeHeight) return;

    uint32_t probeDimensions = 1 << (pc.currentLevel + 1);
    uint32_t probeRayCount = probeDimensions * probeDimensions;
    int2 probe = id.xy / probeDimensions; // probe position
    int2 probeCount = outputCascadeResolution / probeDimensions;

    int rayID = id.x % probeDimensions + (id.y % probeDimensions) * probeDimensions;

    // Build ray
    float rayLength = pc.radius * pow(pc.radiusMultiplier, pc.currentLevel);

    // start offset
    float rayStartOffset = 0;
    for (int i = 0; i < pc.currentLevel; i++) {
        rayStartOffset += pc.radius * pow(pc.radiusMultiplier, i);
    }

    float angle = (rayID + .5f) * 2 * 3.141592653589793 / probeRayCount;
    float2 rayDirection = float2(cos(angle)/ inputSDFAspectRatio, sin(angle));// correct ray direction because UV is slightly warped

    // The probe texture is slightly bigger on the X axis
    // we need to expand it so the probes arent placed closer together on the X
    float2 origin = (probe + float2(0.5)) / probeCount; // get UV position of probe

    // origin correction
    origin.x -= .5f; // center on X
    origin.x *= outputCascadeAspectRatio / inputSDFAspectRatio; // aspect ratio correction
    origin.x += .5f; // remove centering to get uv back

    float4 result = float4(0, 0, 0, 0.0f);
    for (float i = rayStartOffset; i < rayStartOffset + rayLength; i += pc.raymarchStepSize) {
        float2 pos = origin + rayDirection * i;

        float4 color = inputSDF.SampleLevel(pos, 0);

        if (color.a <= 0) {
            result = float4(color.rgb/(i * pc.attenuation), 1.0f);
            result = saturate(result);
            break;
        }
    }

    outputCascade[id.xy] = result;
    // outputCascade[id.xy] = float4(rayID, 0, 0, 0);
}
