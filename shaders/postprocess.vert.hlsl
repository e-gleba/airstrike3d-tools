// Full-screen triangle vertex shader for post-processing
// Uses vertex ID to generate positions without a vertex buffer

struct VertexOutput
{
    float4 position : SV_Position;
    float2 texcoord : TEXCOORD0;
};

VertexOutput main(uint vertex_id : SV_VertexID)
{
    VertexOutput output;
    
    // Generate full-screen triangle from vertex ID
    // Vertex 0: (-1, -1), Vertex 1: (3, -1), Vertex 2: (-1, 3)
    // This covers the entire screen with a single triangle
    float2 uv = float2((vertex_id << 1) & 2, vertex_id & 2);
    output.position = float4(uv * 2.0 - 1.0, 0.0, 1.0);
    output.texcoord = float2(uv.x, 1.0 - uv.y); // Flip Y for texture coords
    
    return output;
}

