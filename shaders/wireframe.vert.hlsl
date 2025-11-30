struct VertexInput
{
    float3 position : POSITION;
    float3 color : COLOR;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float3 color : COLOR;
};

cbuffer UniformBlock : register(b0, space1)
{
    float4x4 view_proj;
};

VertexOutput main(VertexInput input)
{
    VertexOutput output;
    output.position = mul(view_proj, float4(input.position, 1.0));
    output.color = input.color;
    return output;
}

