demo_04_shaders = [
  [ 'shaders/material.glsl',  [ 'ms', 'fs' ] ],
  [ 'shaders/render.task',    [ 'ts' ] ],
  [ 'shaders/present.comp',   [ 'cs' ] ],
]

demo_04_resources = files(
  'resources/resources.json'
)

demo_04_meshlet_spv = []

foreach s : demo_04_shaders
  shader_file = s[0]
  shader_types = s[1]

  foreach p : shader_types
    demo_04_meshlet_spv += custom_target('@0@_@1@'.format(p, shader_file.underscorify()),
      command       : [ glsl_compiler, as_shader_type_args[p] ],
      depend_files  : [ as_shader_includes ],
      input         : files(shader_file),
      output        : '@0@_@BASENAME@.spv'.format(p))
  endforeach
endforeach

if (spv_opt.found())
#  demo_04_meshlet_spv = spv_optimizer.process(demo_04_meshlet_spv)
endif

demo_04_meshlet_shaders_asa = custom_target('demo_04_meshlet_shaders',
  output      : 'demo_04_meshlet_shaders.asa',
  input       : demo_04_meshlet_spv,
  command     : [ asarc, '-o', '@OUTPUT@', '-s', '@INPUT@' ])

demo_04_meshlet_data_asa = custom_target('demo_04_meshlet_data',
  output      : 'demo_04_meshlet_data.asa',
  input       : demo_04_resources,
  command     : [ asarc, '-o', '@OUTPUT@', '-I', '@CURRENT_SOURCE_DIR@/resources', '-j', '@INPUT@' ])

custom_target('demo_04_meshlet_resources',
  output      : 'demo_04_meshlet_resources.asa',
  input       : [ demo_04_meshlet_shaders_asa, demo_04_meshlet_data_asa ],
  command     : [ asarc, '-o', '@OUTPUT@', '-a', '@INPUT@' ],
  install     : true,
  install_dir : 'demos/resources')

executable('04-meshlet', files('main.cpp'),
  link_with   : lib_alseid,
  install     : true,
  install_dir : 'demos')
