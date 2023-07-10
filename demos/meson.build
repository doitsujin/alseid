spv_shader_generator = generator(glsl_compiler,
  output    : [ '@BASENAME@.spv' ],
  arguments : [ '--target-env', 'vulkan1.3', '@INPUT@', '-o', '@OUTPUT@' ])

subdir('01-initialization')
subdir('02-cube')
subdir('03-raytracing')
subdir('04-meshlet')
