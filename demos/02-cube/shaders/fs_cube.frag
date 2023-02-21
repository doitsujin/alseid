#version 450

#extension GL_EXT_nonuniform_qualifier : require

layout(set = 0, binding = 0) uniform texture2D r_textures[];
layout(set = 1, binding = 2) uniform sampler r_sampler;

layout(location = 0) in vec3 i_normal;
layout(location = 1) in vec2 i_coord;

layout(location = 0) out vec4 o_color;

layout(push_constant)
uniform push_t {
  uint textureIndex;
};

void main() {
  vec3 color = texture(sampler2D(r_textures[textureIndex], r_sampler), i_coord).xyz;
  o_color = vec4(normalize(i_normal).z * color, 1.0f);
}
