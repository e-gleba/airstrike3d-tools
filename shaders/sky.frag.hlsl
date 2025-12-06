/// Sky fragment shader with gradient, sun, and procedural clouds
/// Inspired by Shadertoy techniques

struct PixelInput
{
    float2 uv : TEXCOORD0;
    float3 view_dir : TEXCOORD1;
};

cbuffer SkyUniforms : register(b0, space1)
{
    float4x4 inv_view_proj;
    float4 params; // x=time, y=cloud_density, z=sun_intensity, w=sky_preset
};

// Hash function for noise
float hash(float2 p)
{
    return frac(sin(dot(p, float2(127.1, 311.7))) * 43758.5453);
}

// 2D noise
float noise(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);
    float2 u = f * f * (3.0 - 2.0 * f);
    
    return lerp(lerp(hash(i + float2(0,0)), hash(i + float2(1,0)), u.x),
                lerp(hash(i + float2(0,1)), hash(i + float2(1,1)), u.x), u.y);
}

// Fractal Brownian Motion for clouds
float fbm(float2 p)
{
    float f = 0.0;
    float w = 0.5;
    for (int i = 0; i < 5; i++)
    {
        f += w * noise(p);
        p *= 2.0;
        w *= 0.5;
    }
    return f;
}

// Cloud density at a point
float clouds(float2 uv, float time, float density)
{
    float2 q = uv * 2.0 + float2(time * 0.02, time * 0.01);
    float n = fbm(q);
    n = smoothstep(0.4 - density * 0.3, 0.7, n);
    return n;
}

float4 main(PixelInput input) : SV_Target0
{
    float3 dir = normalize(input.view_dir);
    float time = params.x;
    float cloud_density = params.y;
    float sun_intensity = params.z;
    int sky_preset = (int)params.w;
    
    // Height-based gradient (y is up in view direction)
    float height = dir.y * 0.5 + 0.5;
    height = saturate(height);
    
    // Sky colors based on preset
    float3 sky_top, sky_horizon, sun_color;
    float3 sun_dir;
    
    if (sky_preset == 0) // Dark/Night
    {
        sky_top = float3(0.02, 0.02, 0.08);
        sky_horizon = float3(0.08, 0.06, 0.12);
        sun_color = float3(0.8, 0.85, 1.0);
        sun_dir = normalize(float3(0.3, -0.2, 0.5)); // Moon low
        sun_intensity *= 0.3;
    }
    else if (sky_preset == 1) // Day
    {
        sky_top = float3(0.2, 0.4, 0.8);
        sky_horizon = float3(0.6, 0.75, 0.95);
        sun_color = float3(1.0, 0.95, 0.8);
        sun_dir = normalize(float3(0.5, 0.8, 0.3));
    }
    else if (sky_preset == 2) // Sunset
    {
        sky_top = float3(0.15, 0.25, 0.5);
        sky_horizon = float3(0.95, 0.5, 0.3);
        sun_color = float3(1.0, 0.6, 0.2);
        sun_dir = normalize(float3(0.8, 0.15, 0.2));
    }
    else // Night
    {
        sky_top = float3(0.01, 0.01, 0.03);
        sky_horizon = float3(0.05, 0.03, 0.08);
        sun_color = float3(0.7, 0.75, 1.0);
        sun_dir = normalize(float3(-0.3, 0.4, 0.6));
        sun_intensity *= 0.2;
    }
    
    // Gradient sky
    float3 sky = lerp(sky_horizon, sky_top, pow(height, 0.6));
    
    // Add subtle horizon glow
    float horizon_glow = exp(-abs(dir.y) * 8.0);
    sky += sky_horizon * horizon_glow * 0.3;
    
    // Sun disc
    float sun_dot = max(dot(dir, sun_dir), 0.0);
    float sun_disc = pow(sun_dot, 800.0) * sun_intensity * 2.0;
    float sun_glow = pow(sun_dot, 8.0) * sun_intensity * 0.5;
    sky += sun_color * (sun_disc + sun_glow);
    
    // Clouds - only render above horizon
    if (dir.y > 0.0 && cloud_density > 0.01)
    {
        // Project to cloud plane
        float cloud_height = 3.0 / max(dir.y, 0.001);
        float2 cloud_uv = dir.xz * cloud_height;
        
        float c = clouds(cloud_uv, time, cloud_density);
        
        // Cloud color - lit by sun
        float cloud_light = max(dot(float3(0, 1, 0), sun_dir), 0.0) * 0.4 + 0.6;
        float3 cloud_color = lerp(sky_horizon, float3(1, 1, 1), 0.7) * cloud_light;
        
        // Fade clouds near horizon
        float cloud_fade = smoothstep(0.0, 0.3, dir.y);
        c *= cloud_fade;
        
        sky = lerp(sky, cloud_color, c * 0.8);
    }
    
    // Stars for night presets
    if (sky_preset == 0 || sky_preset == 3)
    {
        float2 star_uv = dir.xz / (dir.y + 0.001) * 50.0;
        float stars = step(0.998, hash(floor(star_uv)));
        float star_twinkle = sin(time * 3.0 + hash(floor(star_uv)) * 6.28) * 0.5 + 0.5;
        sky += stars * star_twinkle * (1.0 - cloud_density) * smoothstep(0.1, 0.5, dir.y);
    }
    
    return float4(sky, 1.0);
}

