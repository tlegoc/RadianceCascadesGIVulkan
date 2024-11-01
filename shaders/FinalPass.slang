RWTexture2D<float4> SDF;
[[vk::binding(1)]]
Sampler2D inputGI : register(t2): register(s2);
RWTexture2D<float4> outputTexture;

[shader("compute")]
[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID)
{
    uint width, height, levels;
    outputTexture.GetDimensions(0,width, height, levels);

    uint giWidth, giHeight, giLevels;
    inputGI.GetDimensions(0,giWidth, giHeight, giLevels);
    
    if (id.x >= width || id.y >= height) return;

    float2 uv = float2((float) id.x / width, (float) id.y / height);

    float4 sdfValue = SDF[id.xy];

    float giAspectRatio = (float) giWidth/giHeight;
    float outputAspectRatio = (float) width/height;

    // normalize UVs
    uv.x -= .5f;
    uv.x *= outputAspectRatio / giAspectRatio;
    uv.x += .5f;

    float3 giValue = inputGI.SampleLevel(uv, 0).rgb;

    float3 backgroundColor = float3(0.1);

    outputTexture[id.xy].rgb = lerp(giValue * backgroundColor, sdfValue.rgb,  max(1 - sdfValue.a, 0));
    outputTexture[id.xy].a = 1;
}
