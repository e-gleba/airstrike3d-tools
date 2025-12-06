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
    float res_x;        // Screen resolution X for FXAA
    float res_y;        // Screen resolution Y for FXAA
};

Texture2D scene_texture : register(t0, space2);
SamplerState scene_sampler : register(s0, space2);

// Working FXAA implementation
float3 apply_fxaa(float2 uv, float2 texel_size)
{
    // Sample neighbors
    float3 center = scene_texture.Sample(scene_sampler, uv).rgb;
    float3 n = scene_texture.Sample(scene_sampler, uv + float2(0.0, -texel_size.y)).rgb;
    float3 s = scene_texture.Sample(scene_sampler, uv + float2(0.0, texel_size.y)).rgb;
    float3 e = scene_texture.Sample(scene_sampler, uv + float2(texel_size.x, 0.0)).rgb;
    float3 w = scene_texture.Sample(scene_sampler, uv + float2(-texel_size.x, 0.0)).rgb;
    float3 nw = scene_texture.Sample(scene_sampler, uv + float2(-texel_size.x, -texel_size.y)).rgb;
    float3 ne = scene_texture.Sample(scene_sampler, uv + float2(texel_size.x, -texel_size.y)).rgb;
    float3 sw = scene_texture.Sample(scene_sampler, uv + float2(-texel_size.x, texel_size.y)).rgb;
    float3 se = scene_texture.Sample(scene_sampler, uv + float2(texel_size.x, texel_size.y)).rgb;
    
    // Luminance
    const float3 luma_weights = float3(0.299, 0.587, 0.114);
    float luma_center = dot(center, luma_weights);
    float luma_n = dot(n, luma_weights);
    float luma_s = dot(s, luma_weights);
    float luma_e = dot(e, luma_weights);
    float luma_w = dot(w, luma_weights);
    float luma_nw = dot(nw, luma_weights);
    float luma_ne = dot(ne, luma_weights);
    float luma_sw = dot(sw, luma_weights);
    float luma_se = dot(se, luma_weights);
    
    // Edge detection (more aggressive)
    float luma_min = min(luma_center, min(min(luma_n, luma_s), min(luma_e, luma_w)));
    float luma_max = max(luma_center, max(max(luma_n, luma_s), max(luma_e, luma_w)));
    float luma_range = luma_max - luma_min;
    
    // Early exit if no edge (lower threshold = more aggressive)
    if (luma_range < 0.05)
        return center;
    
    // Edge direction
    float luma_l = luma_nw + luma_w + luma_sw;
    float luma_r = luma_ne + luma_e + luma_se;
    float luma_d = luma_sw + luma_s + luma_se;
    float luma_u = luma_nw + luma_n + luma_ne;
    
    float edge_horiz = abs(luma_l - luma_r);
    float edge_vert = abs(luma_u - luma_d);
    
    bool is_horizontal = edge_horiz >= edge_vert;
    
    // Determine gradient direction
    float luma1 = is_horizontal ? luma_d : luma_l;
    float luma2 = is_horizontal ? luma_u : luma_r;
    float gradient1 = abs(luma1 - luma_center);
    float gradient2 = abs(luma2 - luma_center);
    
    bool is_1_steepest = gradient1 >= gradient2;
    
    // Step direction
    float step_length = is_horizontal ? texel_size.y : texel_size.x;
    if (is_1_steepest)
        step_length = -step_length;
    
    float2 offset = is_horizontal ? float2(0.0, step_length) : float2(step_length, 0.0);
    
    // Sample along edge (more samples for stronger effect)
    float2 uv1 = uv + offset * 0.25;
    float2 uv2 = uv + offset * 0.5;
    float2 uv3 = uv + offset * 0.75;
    float2 uv4 = uv + offset * 1.0;
    float2 uv5 = uv - offset * 0.25;
    
    float3 sample1 = scene_texture.Sample(scene_sampler, uv1).rgb;
    float3 sample2 = scene_texture.Sample(scene_sampler, uv2).rgb;
    float3 sample3 = scene_texture.Sample(scene_sampler, uv3).rgb;
    float3 sample4 = scene_texture.Sample(scene_sampler, uv4).rgb;
    float3 sample5 = scene_texture.Sample(scene_sampler, uv5).rgb;
    
    // Blend samples (weighted towards center samples)
    float3 result = (sample1 * 0.2 + sample2 * 0.3 + sample3 * 0.3 + sample4 * 0.1 + sample5 * 0.1);
    
    // Sub-pixel AA (more aggressive)
    float luma_result = dot(result, luma_weights);
    float luma_avg = (luma_nw + luma_ne + luma_sw + luma_se) * 0.25;
    float subpix = abs(luma_result - luma_avg);
    subpix = saturate(subpix * 3.0); // Increased from 2.0
    
    // More aggressive blending
    return lerp(result, center, subpix * 0.3); // Reduced from 0.5
}

float4 main(PixelInput input) : SV_Target0
{
    float2 uv = input.texcoord;
    float2 resolution = float2(res_x, res_y);
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

