#version 440
// SPDX-License-Identifier: GPL-3.0-only
// Liquid Glass for one Control Center module: a tinted rounded body, a rim that is bright where it faces up and
// faint where it faces down, and a soft shadow in the transparent margin around it. Output is premultiplied.
// Build: qsb --qt6 -o package/contents/ui/shaders/glass.frag.qsb tools/shaders/glass.frag
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    // The glass is boxSize pixels; the item is pad pixels larger on every side, which is the room of the shadow.
    vec2 boxSize;
    float pad;
    // Corner radius in pixels, limited to half the shorter side.
    float radius;
    // Straight alpha; the body is premultiplied below.
    vec4 tint;
    // The rim color where the edge faces up and where it faces down.
    vec4 rimLight;
    vec4 rimShade;
    // Rim thickness in pixels.
    float rimWidth;
    // Light added to the body at its top edge, fading out by three quarters of the height.
    float sheen;
    // Opacity and reach in pixels of the shadow, which is shifted down by 0.3 of its reach.
    float shadowAlpha;
    float shadowSize;
};

// Signed distance in pixels from p to a rounded box centered on the origin: negative inside.
float roundedBox(vec2 p, vec2 halfSize, float r)
{
    vec2 q = abs(p) - halfSize + vec2(r);
    return length(max(q, vec2(0.0))) + min(max(q.x, q.y), 0.0) - r;
}

void main()
{
    vec2 full = boxSize + vec2(2.0 * pad);
    vec2 p = qt_TexCoord0 * full - 0.5 * full;
    float r = min(radius, 0.5 * min(boxSize.x, boxSize.y));
    float d = roundedBox(p, 0.5 * boxSize, r);

    float shadowDistance = roundedBox(p - vec2(0.0, 0.3 * shadowSize), 0.5 * boxSize, r);
    float shadow = shadowAlpha * (1.0 - smoothstep(-0.4 * shadowSize, shadowSize, shadowDistance));
    vec4 outside = vec4(0.0, 0.0, 0.0, shadow);

    // 1 inside the shape, fading to 0 across the pixel at its edge.
    float coverage = clamp(0.5 - d, 0.0, 1.0);

    float y = clamp((p.y + 0.5 * boxSize.y) / boxSize.y, 0.0, 1.0);
    vec3 bodyRgb = tint.rgb + vec3(sheen * (1.0 - smoothstep(0.0, 0.75, y)));
    vec4 body = vec4(bodyRgb * tint.a, tint.a);

    vec2 q = abs(p) - 0.5 * boxSize + vec2(r);
    vec2 s = vec2(p.x < 0.0 ? -1.0 : 1.0, p.y < 0.0 ? -1.0 : 1.0);
    vec2 n = (q.x > 0.0 && q.y > 0.0) ? normalize(q) * s : (q.x > q.y ? vec2(s.x, 0.0) : vec2(0.0, s.y));
    // n is the outward direction of the nearest edge; facing is 1 on the top edge and 0 on the bottom edge.
    float facing = clamp(0.5 - 0.5 * n.y, 0.0, 1.0);
    vec4 rimColour = mix(rimShade, rimLight, facing * facing);
    float band = 1.0 - smoothstep(rimWidth - 0.6, rimWidth + 0.6, -d);
    vec4 rim = vec4(rimColour.rgb * rimColour.a, rimColour.a) * band;

    vec4 glass = rim + body * (1.0 - rim.a);
    fragColor = (glass * coverage + outside * (1.0 - coverage)) * qt_Opacity;
}
