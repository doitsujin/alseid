spv_shader_generator = generator(glsl_compiler,
  output    : [ '@BASENAME@.spv' ],
  arguments : [ '--target-env', 'vulkan1.3', '@INPUT@', '-o', '@OUTPUT@' ])

asa_shader_generator = generator(asshader,
  output    : [ '@BASENAME@.asa' ],
  arguments : [ '@OUTPUT', '@INPUT@' ])

subdir('01-initialization')
subdir('02-cube')
subdir('03-raytracing')
