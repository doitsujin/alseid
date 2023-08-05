project('alseid', ['cpp', 'c'], default_options : 'cpp_std=c++20')

glsl_compiler = find_program('glslangValidator')
glsl_generator = generator(glsl_compiler,
  output    : [ '@BASENAME@.h' ],
  arguments : [ '--target-env', 'vulkan1.3', '--vn', '@BASENAME@', '@INPUT@', '-o', '@OUTPUT@' ])

spv_opt = find_program('spirv-opt', required : false)

if (spv_opt.found())
  spv_optimizer = generator(spv_opt,
    output    : [ '@BASENAME@.spv' ],
    arguments : [
      '--skip-block-layout',
      '--strip-debug',
      '--eliminate-dead-branches',
      '--eliminate-dead-functions',
      '--strength-reduction',
      '--eliminate-dead-code-aggressive',
      '--eliminate-dead-branches',
      '--eliminate-dead-functions',
      '--eliminate-dead-variables',
      '--eliminate-dead-const',
      '--merge-blocks',
      '--cfg-cleanup',
      '--remove-duplicates',
      '--compact-ids',
      '-o', '@OUTPUT@', '@INPUT@' ])
endif

cpp_compiler = meson.get_compiler('cpp')

cmake = import('cmake')

spirv_cross_proj = cmake.subproject('SPIRV-Cross')
spirv_cross_core_dep = spirv_cross_proj.dependency('spirv-cross-core')
spirv_cross_glsl_dep = spirv_cross_proj.dependency('spirv-cross-glsl')
spirv_cross_hlsl_dep = spirv_cross_proj.dependency('spirv-cross-hlsl')

meshoptimizer_proj = cmake.subproject('meshoptimizer')
meshoptimizer_dep = meshoptimizer_proj.dependency('meshoptimizer')

subdir('src')
subdir('tools')
subdir('demos')
