// Sprite.hlsl - SpriteBatch quad shader (phase 5, moved to a file in phase 6).
// Comments are ASCII on purpose: D3DCompileFromFile reads the source as ANSI.
//
// VS : logical coordinates -> NDC through the orthographic projection in b0. z is 0 (no depth buffer;
//      NDC z only has to stay inside 0..1).
// PS : texture sample * vertex colour (tint). Both the texture (32bppPRGBA) and the tint are
//      premultiplied, so the product is premultiplied too. Solid rectangles sample a 1x1 white
//      texture, so this single shader covers them as well.

cbuffer SpriteConstants : register(b0)
{
    float4x4 projection;   // logical coordinates -> NDC (transposed on the CPU side)
};

Texture2D    spriteTexture : register(t0);
SamplerState spriteSampler : register(s0);

struct VSIn  { float2 pos : POSITION; float2 uv : TEXCOORD0; float4 color : COLOR0; };
struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; float4 color : COLOR0; };

VSOut VS(VSIn i)
{
    VSOut o;
    o.pos   = mul(float4(i.pos, 0.0, 1.0), projection);
    o.uv    = i.uv;
    o.color = i.color;
    return o;
}

float4 PS(VSOut i) : SV_Target
{
    // both the texture (32bppPRGBA) and the vertex color are premultiplied, so the product is too
    return spriteTexture.Sample(spriteSampler, i.uv) * i.color;
}
