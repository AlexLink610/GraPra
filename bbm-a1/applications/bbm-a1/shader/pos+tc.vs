#version 430
layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec2 in_tc;
uniform mat4 proj;
uniform mat4 view;
uniform mat4 model;
out vec2 tc;
void main() {
    gl_Position = proj * view * model * vec4(in_pos, 1.);
    tc = in_tc;
}
