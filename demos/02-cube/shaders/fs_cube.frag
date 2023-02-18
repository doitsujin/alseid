#version 450

layout(location = 0) in vec3 i_normal;

layout(location = 0) out vec4 o_color;

void main() {
  o_color = vec4(0.5f * i_normal + 0.5f, 1.0f);
}
