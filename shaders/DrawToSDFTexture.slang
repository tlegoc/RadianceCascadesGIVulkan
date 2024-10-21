RWTexture2D<float4> outputTexture;

[shader("compute")]
[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID, uniform int16_t mousePosX, uniform int16_t mousePosY, uniform uint8_t radius, uniform uint8_t r, uniform uint8_t g, uniform uint8_t b)
{
    uint width, height, levels;
    outputTexture.GetDimensions(0,width, height, levels);
    
    if (id.x >= width || id.y >= height) return;
    if (mousePosX < 0 || mousePosY < 0 || mousePosX >= width || mousePosY >= height) return;

    int2 mousePos = int2(mousePosX, mousePosY);
    float dist = max(distance(id.xy, mousePos) - radius, 0.0f);

    if ((outputTexture[id.xy].a == 0 && dist == 0) || 1.0f/(dist + 0.00000001f) > 1.0f/(outputTexture[id.xy].a + 0.00000001f)) {
        outputTexture[id.xy] = float4(r/256.0f, g/256.0f, b/256.0f, dist);
    }
}
