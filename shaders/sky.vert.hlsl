/// Sky vertex shader - fullscreen triangle
/// Uses vertex ID to generate positions without vertex buffer

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float3 view_dir : TEXCOORD1;
};

cbuffer SkyUniforms : register(b0, space1)
{
    float4x4 inv_view_proj;
    float4 params; // x=time, y=cloud_density, z=sun_intensity, w=unused
};

VertexOutput main(uint vertex_id : SV_VertexID)
{
    VertexOutput output;
    
    // Generate fullscreen triangle from vertex ID
    // Positions: (-1,-1), (3,-1), (-1,3)
    float2 uv = float2((vertex_id << 1) & 2, vertex_id & 2);
    output.position = float4(uv * 2.0 - 1.0, 0.0, 1.0);
    output.uv = uv;
    
    // Calculate view direction for sky sampling
    float4 clip_pos = float4(output.position.xy, 1.0, 1.0);
    float4 world_pos = mul(inv_view_proj, clip_pos);
    output.view_dir = normalize(world_pos.xyz / world_pos.w);
    
    return output;
}

