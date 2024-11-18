RWTexture2D<float4> outputTexture;

struct PushConstant {
    uint32_t mousePosX;
    uint32_t mousePosY;
    uint32_t radius;
    uint32_t rgba;
};

vector<uint8_t, 4> UnpackRGBA(uint32_t rgb) {
    return vector<uint8_t, 4>(rgb & 0xFF, (rgb >> 8) & 0xFF, (rgb >> 16) & 0xFF, (rgb >> 24) & 0xFF);
}

[shader("compute")]
[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID, uniform PushConstant pc)
{
    uint width, height, levels;
    outputTexture.GetDimensions(0,width, height, levels);
    
    if (id.x >= width || id.y >= height) return;
    if (pc.mousePosX < 0 || pc.mousePosY < 0 || pc.mousePosX >= width || pc.mousePosY >= height) return;

    int2 mousePos = int2(pc.mousePosX, pc.mousePosY);
    float dist = max(distance(id.xy, mousePos) - pc.radius, 0.0f);

    vector<uint8_t, 4> rgba = UnpackRGBA(pc.rgba);
    float r = rgba[0];
    float g = rgba[1];
    float b = rgba[2];

    if ((outputTexture[id.xy].a == 0 && dist == 0) || 1.0f/(dist + 0.00000001f) > 1.0f/(outputTexture[id.xy].a + 0.00000001f)) {
        outputTexture[id.xy] = float4(r/256.0f, g/256.0f, b/256.0f, dist);
    }
}
