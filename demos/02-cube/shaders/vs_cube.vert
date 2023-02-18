#version 450

layout(location = 0) in vec3 i_position;
layout(location = 1) in vec3 i_normal;

layout(location = 0) out vec3 o_normal;

layout(set = 0, binding = 0, std140)
uniform global_t {
  mat4 projMatrix;
  mat4 viewMatrix;
};

layout(set = 1, binding = 0, std140)
uniform model_t {
  mat4 modelMatrix;
};

invariant gl_Position;

void main() {
  o_normal = mat3(modelMatrix) * i_normal;

  gl_Position = projMatrix * viewMatrix * modelMatrix * vec4(i_position, 1.0f);
}
