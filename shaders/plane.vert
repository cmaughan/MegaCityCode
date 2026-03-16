#version 450

layout(push_constant) uniform ScenePushConstants {
    mat4 view_proj;
    vec4 material_color;
} pc;

layout(location = 0) out vec2 frag_uv;
layout(location = 1) out vec3 frag_world_pos;

void main() {
    vec2 positions[6] = vec2[](
        vec2(-4.0, -4.0),
        vec2( 4.0, -4.0),
        vec2(-4.0,  4.0),
        vec2( 4.0, -4.0),
        vec2( 4.0,  4.0),
        vec2(-4.0,  4.0)
    );

    vec2 uvs[6] = vec2[](
        vec2(0.0, 0.0),
        vec2(1.0, 0.0),
        vec2(0.0, 1.0),
        vec2(1.0, 0.0),
        vec2(1.0, 1.0),
        vec2(0.0, 1.0)
    );

    vec2 plane = positions[gl_VertexIndex];
    vec3 world_pos = vec3(plane.x, 0.0, plane.y);

    frag_uv = uvs[gl_VertexIndex];
    frag_world_pos = world_pos;
    gl_Position = pc.view_proj * vec4(world_pos, 1.0);
}
