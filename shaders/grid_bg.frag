#version 450

layout(location = 0) in vec4 frag_bg;
layout(location = 0) out vec4 out_color;

void main() {
    out_color = frag_bg;
}
