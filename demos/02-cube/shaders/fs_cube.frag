#version 450

#extension GL_EXT_fragment_shading_rate : require
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
  if (gl_ShadingRateEXT == 5)
    o_color.xyz = vec3(0.0f, 1.0f, 0.0f);
  else if (gl_ShadingRateEXT == 4)
    o_color.xyz = vec3(0.0f, 0.0f, 1.0f);
  else
    o_color.xyz = vec3(1.0f, 0.0f, 0.0f);
  o_color.w = 1.0f;
}
