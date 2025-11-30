struct PixelInput
{
    float3 color : COLOR;
};

float4 main(PixelInput input) : SV_Target0
{
    return float4(input.color, 1.0);
}
