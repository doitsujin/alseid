demo_02_cube_shaders = files(
  'shaders/cs_present.comp',
  'shaders/fs_cube.frag',
  'shaders/vs_cube.vert',
)

demo_02_cube_spv = spv_shader_generator.process(demo_02_cube_shaders)

custom_target('demo_02_cube_shaders',
  output      : 'demo_02_cube_shaders.asa',
  input       : demo_02_cube_spv,
  command     : [ asshader, '@OUTPUT@', '@INPUT@' ],
  install     : true,
  install_dir : 'demos/resources')

executable('02-cube', files('main.cpp'),
  link_with   : lib_alseid,
  install     : true,
  install_dir : 'demos')
