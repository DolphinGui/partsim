#version 450

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 fragColor;

layout(binding = 0) uniform UniformBufferObject {
    vec2 dir;
} ubo;

void main() {
    gl_Position = vec4(inPosition - ubo.dir, 0.0, 1.0);
    fragColor = inColor;
}