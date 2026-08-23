// TestPattern.hlsl - procedural resize / letterbox verification pattern (phase 1.5 / 4).
// Moved out of TestPattern.cpp in phase 6 (ShaderManager). Comments are ASCII on purpose:
// D3DCompileFromFile reads the source as ANSI, so UTF-8 Korean would trigger X3000 warnings.
//
// VS : builds one big triangle from SV_VertexID (0,1,2) that covers the whole screen in NDC.
//      (-1,-1) (-1,3) (3,-1) - the parts outside the screen are clipped by the rasterizer.
//      No vertex buffer, no input layout. One triangle instead of a quad avoids shading the
//      diagonal edge pixels twice. Winding (-1,-1)->(-1,3)->(3,-1) is clockwise = front face
//      under the default rasterizer state, so it is not culled.
// PS : SV_Position is the back-buffer physical pixel coordinate (pixel centre at .5).
//      Subtracting the viewport origin turns it into a viewport-relative coordinate, so the
//      pattern is drawn relative to the letterbox area (phase 4).

cbuffer PatternConstants : register(b0)
{
    float4 viewport;   // x, y, width, height (physical pixels)
};

struct VSOut { float4 pos : SV_Position; };

VSOut VS(uint id : SV_VertexID)
{
    float2 p = float2((id == 2) ? 3.0 : -1.0, (id == 1) ? 3.0 : -1.0);
    VSOut o;
    o.pos = float4(p, 0.0, 1.0);
    return o;
}

float4 PS(VSOut i) : SV_Target
{
    float2 px   = i.pos.xy - viewport.xy;   // viewport-relative pixel coordinate
    float2 size = viewport.zw;

    // 1px checkerboard
    float checker = fmod(floor(px.x) + floor(px.y), 2.0);
    float3 color  = lerp(float3(0.15, 0.15, 0.15), float3(0.85, 0.85, 0.85), checker);

    // center ring (radius = 40% of the shorter side, 4px thick)
    float2 center = size * 0.5;
    float  radius = min(size.x, size.y) * 0.4;
    float  dist   = length(px - center);
    if (abs(dist - radius) < 2.0) color = float3(1.0, 0.2, 0.2);

    // crosshair (2px)
    if (abs(px.x - center.x) < 1.0 || abs(px.y - center.y) < 1.0) color = float3(0.2, 1.0, 0.2);

    // border (4px)
    if (px.x < 4.0 || px.y < 4.0 || px.x >= size.x - 4.0 || px.y >= size.y - 4.0) color = float3(0.2, 0.6, 1.0);

    return float4(color, 1.0);
}
