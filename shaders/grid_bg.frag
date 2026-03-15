#version 450

layout(location = 0) in vec4 frag_bg;
layout(location = 1) in vec4 frag_fg;
layout(location = 2) in vec4 frag_sp;
layout(location = 3) in vec2 frag_local_uv;
layout(location = 4) flat in uint frag_style_flags;
layout(location = 0) out vec4 out_color;

void main() {
    vec4 color = frag_bg;
    bool underline = (frag_style_flags & 4u) != 0u;
    bool strikethrough = (frag_style_flags & 8u) != 0u;
    bool undercurl = (frag_style_flags & 16u) != 0u;
    vec4 accent = frag_sp.a > 0.0 ? frag_sp : frag_fg;

    if (underline && frag_local_uv.y >= 0.86 && frag_local_uv.y <= 0.93)
        color = accent;
    else if (strikethrough && frag_local_uv.y >= 0.48 && frag_local_uv.y <= 0.54)
        color = frag_fg;
    else if (undercurl) {
        float baseline = 0.84 + 0.05 * sin(frag_local_uv.x * 18.8495559);
        if (abs(frag_local_uv.y - baseline) <= 0.03)
            color = accent;
    }

    out_color = color;
}
