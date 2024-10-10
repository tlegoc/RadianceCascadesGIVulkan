RWTexture2D<float4> outputTexture;

[shader("compute")]
[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID, uniform float4 value)
{
    // if (id.x >= width || id.y >= height) return; uniform uint16_t width, uniform uint16_t height,

    outputTexture[id.xy] = value;
}
