#version 430
layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec2 in_tc;
uniform mat4 model;
uniform mat4 view;
uniform mat4 proj;
out vec2 tc;
void main() {
    tc = in_tc;
    mat4 v = view;
    v[3]= vec4(0,0,0,1); // hacky: sky is at inf --> remove view dependent translation
    gl_Position = (proj * v * model * vec4(in_pos, 1.0)).xyww;
}
