#version 450

layout(push_constant) uniform ScenePushConstants {
    mat4 view_proj;
    vec4 material_color;
} pc;

layout(location = 0) in vec2 frag_uv;
layout(location = 1) in vec3 frag_world_pos;

layout(location = 0) out vec4 out_color;

void main() {
    vec3 normal = vec3(0.0, 1.0, 0.0);
    vec3 light_dir = normalize(vec3(-0.45, 1.0, 0.30));
    float diffuse = 0.35 + 0.65 * max(dot(normal, light_dir), 0.0);

    vec2 tiled_uv = frag_uv * 10.0;
    float checker = mod(floor(tiled_uv.x) + floor(tiled_uv.y), 2.0);
    vec3 base_a = pc.material_color.rgb * vec3(0.82, 0.85, 0.88);
    vec3 base_b = pc.material_color.rgb * vec3(1.05, 1.00, 0.94);
    vec3 albedo = mix(base_a, base_b, checker);

    vec2 line_dist = abs(fract(tiled_uv - 0.5) - 0.5) / fwidth(tiled_uv);
    float grid_line = 1.0 - clamp(min(line_dist.x, line_dist.y), 0.0, 1.0);
    vec3 shaded = albedo * diffuse;
    shaded = mix(shaded, vec3(0.16, 0.18, 0.20), grid_line * 0.18);

    float center_glow = exp(-0.12 * dot(frag_world_pos.xz, frag_world_pos.xz));
    shaded += vec3(0.06, 0.05, 0.04) * center_glow;

    out_color = vec4(shaded, 1.0);
}
