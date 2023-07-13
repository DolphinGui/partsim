#version 450

layout(location = 0) in vec2 inPosition;

layout(location = 0) out vec3 fragColor;

layout(binding = 0) uniform UniformBufferObject {
    vec2 dir[2];
} ubo;

layout (constant_id = 0) const float scale_x = 1.0;
layout (constant_id = 1) const float scale_y = 1.0;
const vec2 scale = vec2(scale_x, scale_y);

void main() {
    gl_Position =  vec4(scale * (inPosition - ubo.dir[gl_InstanceIndex]), 0.0, 1.0);
    fragColor = vec3(0.2, 0.2, 0.2);
}