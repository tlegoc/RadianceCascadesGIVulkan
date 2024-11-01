RWTexture2D<float4> inputCascade;
RWTexture2D<float4> outputCascade;

struct PushConstants {
    int maxLevel;
    int verticalProbeCountAtMaxLevel;
    float radius;
    float radiusMultiplier;
    float raymarchStepSize;
    float attenuation;
    int outputLevel;
};

float4 SampleProbe(int2 probe, int dimension, int rayCount, int rays[4])
{
    int width, height, level;
    inputCascade.GetDimensions(0, width, height, level);

    float4 result = float4(0);

    for (int i = 0; i < 4; i++)
    {
        int rayIndex = rays[i] % rayCount;
        int rayX = rayIndex % dimension;
        int rayY = rayIndex / dimension;

        int probeX = probe.x * dimension + rayX;
        int probeY = probe.y * dimension + rayY;

        if (probeX >= width || probeY >= height) continue;

        result += inputCascade[int2(probeX, probeY)];
    }
    
    return result / 4.0f;
}

[shader("compute")]
[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID, uniform PushConstants pc)
{
    uint width, height, levels;
    inputCascade.GetDimensions(0,width, height, levels);
    int2 cascadeResolution = int2(width, height);

    if (id.x >= width || id.y >= height) return;
    
    int probeDimensions = 1 << (pc.outputLevel + 1);
    int probeRayCount = probeDimensions * probeDimensions;
    int2 probeCount = cascadeResolution / probeDimensions;
    int2 probePosition = id.xy / probeDimensions;

    int rayID = id.x % probeDimensions + (id.y % probeDimensions) * probeDimensions;

    int inputProbeDimension = 1 << (pc.outputLevel + 2);
    int inputProbeRayCount = inputProbeDimension * inputProbeDimension;
    int2 inputProbeCount = cascadeResolution / inputProbeDimension;

    // find the two rays that we'll need to lerp
    // https://github.com/simondevyoutube/Shaders_RadianceCascades/blob/bba7867d1c0f1f0043c0ad618c6967d06d92c11e/shaders/cascades.glsl#L64
    float angleNorm = (rayID + .5f) / (float) probeRayCount;
    int angleIndex = floor(angleNorm * inputProbeRayCount);
    int rays[4] = {
        angleIndex - 1,
        angleIndex,
        angleIndex + 1,
        angleIndex + 2
    };

    // find probes to interpolate
    float2 outputProbePositionInInput = ((float2) probePosition / probeCount) * (float2) inputProbeCount - float2(0.25f);

    int2 probe1 = ceil(outputProbePositionInInput);
    int2 probe2 = floor(outputProbePositionInInput);
    int2 probe3 = int2(ceil(outputProbePositionInInput.x), floor(outputProbePositionInInput.y));
    int2 probe4 = int2(floor(outputProbePositionInInput.x), ceil(outputProbePositionInInput.y));

    // bilinear interpolation
    float2 lerpWeights = outputProbePositionInInput - probe2;
    float4 probe1Value = SampleProbe(probe1, inputProbeDimension, inputProbeRayCount, rays);
    float4 probe2Value = SampleProbe(probe2, inputProbeDimension, inputProbeRayCount, rays);
    float4 probe3Value = SampleProbe(probe3, inputProbeDimension, inputProbeRayCount, rays);
    float4 probe4Value = SampleProbe(probe4, inputProbeDimension, inputProbeRayCount, rays);

    float4 lerp1 = lerp(probe1Value, probe2Value, lerpWeights.y);
    float4 lerp2 = lerp(probe3Value, probe4Value, lerpWeights.y);

    float4 finalValue = lerp(lerp1, lerp2, lerpWeights.x);
    //finalValue = float4(ray1 + ray2, 0, 0, 0);

    outputCascade[id.xy] += finalValue * outputCascade[id.xy].a;
    // outputCascade[id.xy] = float4(angleIndex, 0, 0, 0);
}
