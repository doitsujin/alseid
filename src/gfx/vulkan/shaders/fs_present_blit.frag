#version 450

#extension GL_EXT_samplerless_texture_functions : require

layout(set = 0, binding = 0) uniform texture2D r_src;

layout(location = 0) out vec4 o_color;

void main() {
  o_color = texelFetch(r_src, ivec2(gl_FragCoord.xy), 0);
}
