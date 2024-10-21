RWTexture2D<float4> outputTexture;

[shader("compute")]
[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID, uniform float4 value)
{
    uint width, height, levels;
    outputTexture.GetDimensions(0,width, height, levels);
    if (id.x >= width || id.y >= height) return;
    
    outputTexture[id.xy] = value;

}
