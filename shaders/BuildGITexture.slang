RWTexture2D<float4> inputCascade;
RWTexture2D<float4> output;

[shader("compute")]
[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID)
{
    uint width, height, levels;
    output.GetDimensions(0,width, height, levels);

    if (id.x >= width || id.y >= height) return;

    output[id.xy] = (inputCascade[id.xy * 2] + inputCascade[id.xy * 2 + int2(0, 1)] + inputCascade[id.xy * 2 + int2(1, 0)] + inputCascade[id.xy * 2 + int2(1, 1)]) / 4.0f;
}
