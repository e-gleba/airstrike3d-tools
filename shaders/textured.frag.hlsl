struct PixelInput
{
    float3 normal : NORMAL;
    float2 texcoord : TEXCOORD;
};

Texture2D tex : register(t0, space2);
SamplerState samp : register(s0, space2);

float4 main(PixelInput input) : SV_Target0
{
    float4 tex_color = tex.Sample(samp, input.texcoord);
    
    // Alpha test - discard transparent pixels
    if (tex_color.a < 0.5)
        discard;
    
    // Simple lighting
    float3 light_dir = normalize(float3(0.5, 1.0, 0.3));
    float ndotl = max(dot(normalize(input.normal), light_dir), 0.0);
    float light = 0.4 + ndotl * 0.6;
    
    return float4(tex_color.rgb * light, 1.0);
}
