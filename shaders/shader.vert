#version 450

layout(location = 0) in vec2 inPosition;

layout(location = 0) out vec3 fragColor;


layout (constant_id = 0) const float scale_x = 1.0;
layout (constant_id = 1) const float scale_y = 1.0;

layout(constant_id = 2) const uint count = 4;
const uint size = 500;
const vec2 scale = vec2(scale_x, scale_y);

layout(binding = 0, std430) buffer worldstate{
    vec2 pos[size];
    vec2 vel[size];
  vec4 color[size];
};

layout( push_constant ) uniform constants {
    mat4 render_matrix;
} push;

void main() {
    gl_Position =  push.render_matrix * 
    vec4(scale * (inPosition - pos[gl_InstanceIndex]), 0.0, 1.0);
    fragColor = color[gl_InstanceIndex].xyz;
}