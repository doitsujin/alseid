project('alseid', ['cpp', 'c'], default_options : 'cpp_std=c++20')

glsl_compiler = find_program('glslangValidator')
glsl_generator = generator(glsl_compiler,
  output    : [ '@BASENAME@.h' ],
  arguments : [ '--target-env', 'vulkan1.3', '--vn', '@BASENAME@', '@INPUT@', '-o', '@OUTPUT@' ])

cpp_compiler = meson.get_compiler('cpp')

cmake = import('cmake')

spirv_cross_proj = cmake.subproject('SPIRV-Cross')
spirv_cross_core_dep = spirv_cross_proj.dependency('spirv-cross-core')
spirv_cross_glsl_dep = spirv_cross_proj.dependency('spirv-cross-glsl')
spirv_cross_hlsl_dep = spirv_cross_proj.dependency('spirv-cross-hlsl')

subdir('src')
subdir('demos')
