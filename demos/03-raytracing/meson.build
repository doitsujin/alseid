demo_03_raytracing_shaders = files(
  'shaders/cs_rt.comp',
)

demo_03_raytracing_spv = spv_shader_generator.process(demo_03_raytracing_shaders)

custom_target('demo_03_raytracing_resources',
  output      : 'demo_03_raytracing_resources.asa',
  input       : demo_03_raytracing_spv,
  command     : [ asarc, '-o', '@OUTPUT@', '-s', '@INPUT@' ],
  install     : true,
  install_dir : 'demos/resources')

executable('03-raytracing', files('main.cpp'),
  link_with   : lib_alseid,
  install     : true,
  install_dir : 'demos')
