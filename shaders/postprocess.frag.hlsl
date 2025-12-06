// Post-processing fragment shader
// Applies gamma, brightness, contrast, saturation, vignette, and FXAA

struct PixelInput
{
    float4 position : SV_Position;
    float2 texcoord : TEXCOORD0;
};

// Post-processing parameters (pushed as fragment uniform)
cbuffer PostProcessParams : register(b0, space3)
{
    float gamma;        // Default: 2.2
    float brightness;   // Default: 0.0, range: -1 to 1
    float contrast;     // Default: 1.0, range: 0.5 to 2.0
    float saturation;   // Default: 1.0, range: 0 to 2.0
    float vignette;     // Default: 0.0, range: 0 to 1.0
    float fxaa_enabled; // 0.0 or 1.0
    float2 resolution;  // Screen resolution for FXAA
};

Texture2D scene_texture : register(t0, space2);
SamplerState scene_sampler : register(s0, space2);

// Simple FXAA implementation
float3 apply_fxaa(float2 uv, float2 texel_size)
{
    // Sample center and neighbors
    float3 center = scene_texture.Sample(scene_sampler, uv).rgb;
    float3 n = scene_texture.Sample(scene_sampler, uv + float2(0, -texel_size.y)).rgb;
    float3 s = scene_texture.Sample(scene_sampler, uv + float2(0, texel_size.y)).rgb;
    float3 e = scene_texture.Sample(scene_sampler, uv + float2(texel_size.x, 0)).rgb;
    float3 w = scene_texture.Sample(scene_sampler, uv + float2(-texel_size.x, 0)).rgb;
    
    // Luminance
    float3 luma = float3(0.299, 0.587, 0.114);
    float luma_center = dot(center, luma);
    float luma_n = dot(n, luma);
    float luma_s = dot(s, luma);
    float luma_e = dot(e, luma);
    float luma_w = dot(w, luma);
    
    float luma_min = min(luma_center, min(min(luma_n, luma_s), min(luma_e, luma_w)));
    float luma_max = max(luma_center, max(max(luma_n, luma_s), max(luma_e, luma_w)));
    float luma_range = luma_max - luma_min;
    
    // Skip if contrast is low
    if (luma_range < max(0.0312, luma_max * 0.125))
        return center;
    
    // Compute edge direction
    float2 dir;
    dir.x = -((luma_n + luma_s) - (luma_e + luma_w));
    dir.y = ((luma_n + luma_e) - (luma_s + luma_w));
    
    float dir_reduce = max((luma_n + luma_s + luma_e + luma_w) * 0.25 * 0.25, 0.0078125);
    float rcp_dir_min = 1.0 / (min(abs(dir.x), abs(dir.y)) + dir_reduce);
    dir = clamp(dir * rcp_dir_min, float2(-8, -8), float2(8, 8)) * texel_size;
    
    // Sample along edge
    float3 result1 = 0.5 * (
        scene_texture.Sample(scene_sampler, uv + dir * (1.0/3.0 - 0.5)).rgb +
        scene_texture.Sample(scene_sampler, uv + dir * (2.0/3.0 - 0.5)).rgb
    );
    float3 result2 = result1 * 0.5 + 0.25 * (
        scene_texture.Sample(scene_sampler, uv + dir * -0.5).rgb +
        scene_texture.Sample(scene_sampler, uv + dir * 0.5).rgb
    );
    
    float luma_result2 = dot(result2, luma);
    if (luma_result2 < luma_min || luma_result2 > luma_max)
        return result1;
    return result2;
}

float4 main(PixelInput input) : SV_Target0
{
    float2 uv = input.texcoord;
    float2 texel_size = 1.0 / resolution;
    
    // Sample scene (with optional FXAA)
    float3 color;
    if (fxaa_enabled > 0.5)
    {
        color = apply_fxaa(uv, texel_size);
    }
    else
    {
        color = scene_texture.Sample(scene_sampler, uv).rgb;
    }
    
    // Apply brightness (additive)
    color += brightness;
    
    // Apply contrast (multiply around 0.5)
    color = (color - 0.5) * contrast + 0.5;
    
    // Apply saturation
    float gray = dot(color, float3(0.299, 0.587, 0.114));
    color = lerp(float3(gray, gray, gray), color, saturation);
    
    // Apply vignette
    if (vignette > 0.001)
    {
        float2 center_dist = uv - 0.5;
        float dist = length(center_dist) * 1.414; // Normalize to corner = 1.0
        float vig = 1.0 - smoothstep(0.5, 1.2, dist) * vignette;
        color *= vig;
    }
    
    // Apply gamma correction
    color = pow(max(color, 0.0), 1.0 / gamma);
    
    // Clamp to valid range
    color = saturate(color);
    
    return float4(color, 1.0);
}

