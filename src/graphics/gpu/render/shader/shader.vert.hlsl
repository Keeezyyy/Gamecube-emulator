struct Xform
{
    float4 row0;
    float4 row1;
    float4 row2;
};

StructuredBuffer<float4> matrixRows : register(t0, space0);

cbuffer Camera : register(b0, space1)
{
    column_major float4x4 proj;
};

struct VSInput {
    [[vk::location(0)]] float3 inPos    : POSITION;
    [[vk::location(1)]] uint   inPacked : TEXCOORD0;
};

struct VSOutput {
    float4 position                  : SV_Position;
    [[vk::location(0)]] float4 color : COLOR0;
};

VSOutput main(VSInput input) {
    VSOutput output;
    bool is3d      = (input.inPacked         & 0xFFu) != 0u;
    bool hasMatIdx = ((input.inPacked >> 8u) & 0xFFu) != 0u;
    uint matIdx    =  input.inPacked >> 16u;


    float4 p = float4(input.inPos, 1.0);
    Xform  x;
    x.row0 = matrixRows[matIdx];
    x.row0 = matrixRows[matIdx+1];
    x.row0 = matrixRows[matIdx+2];


   float3 world = float3(dot(x.row0, p),
                          dot(x.row1, p),
                          dot(x.row2, p));

    output.position = mul(proj, float4(world, 1.0));
    output.color    = float4(1.0, 1.0, 1.0, 1.0);
    return output;
}
