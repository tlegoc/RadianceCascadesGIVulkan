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

    float4 sdfValue = float4(SDF[id.xy].rgb, 1.0);

    float giAspectRatio = (float) giWidth/giHeight;
    float outputAspectRatio = (float) width/height;

    // normalize UVs
    uv.x -= .5f;
    uv.x *= outputAspectRatio / giAspectRatio;
    uv.x += .5f;

    float4 giValue = inputGI.SampleLevel(uv, 0);
    giValue.a = 0.0f;

    outputTexture[id.xy] = lerp(giValue, sdfValue, max(1 - SDF[id.xy].a, 0));
}
