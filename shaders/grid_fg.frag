#version 450

layout(set = 0, binding = 1) uniform sampler2D atlas;

layout(location = 0) in vec2 frag_uv;
layout(location = 1) in vec4 frag_fg;

layout(location = 0) out vec4 out_color;

void main() {
    float alpha = texture(atlas, frag_uv).r;
    // Skip fully transparent fragments
    if (alpha < 0.01) discard;
    out_color = vec4(frag_fg.rgb, frag_fg.a * alpha);
}
