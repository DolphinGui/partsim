#version 450

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 fragColor;

layout(binding = 0) uniform UniformBufferObject {
    vec2 dir;
} ubo;

layout (constant_id = 0) const float scale_x = 1.0;
layout (constant_id = 1) const float scale_y = 1.0;
const vec2 scale = vec2(scale_x, scale_y);

void main() {
    gl_Position =  vec4(inPosition * scale - ubo.dir, 0.0, 1.0);
    fragColor = inColor;
}