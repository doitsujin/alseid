#version 450

layout(location = 0) in vec3 i_position;
layout(location = 1) in vec3 i_normal;
layout(location = 2) in vec2 i_coord;

layout(location = 0) out vec3 o_normal;
layout(location = 1) out vec2 o_coord;

layout(set = 1, binding = 0, std140)
uniform global_t {
  mat4 projMatrix;
  mat4 viewMatrix;
};

layout(set = 1, binding = 1, std140)
uniform model_t {
  mat4 modelMatrix;
};

invariant gl_Position;

void main() {
  mat4 modelView = viewMatrix * modelMatrix;
  o_normal = mat3(modelView) * i_normal;
  o_coord = i_coord;

  gl_Position = projMatrix * modelView * vec4(i_position, 1.0f);
}
