demo_02_cube_shaders = files(
  'shaders/cs_present.comp',
  'shaders/fs_cube.frag',
  'shaders/vs_cube.vert',
)

demo_02_cube_spv = spv_shader_generator.process(demo_02_cube_shaders)

demo_02_cube_textures = files(
  'resources/texture.png'
)

demo_02_cube_shaders_asa = custom_target('demo_02_cube_shaders',
  output      : 'demo_02_cube_shaders.asa',
  input       : demo_02_cube_spv,
  command     : [ asarc, '-o', '@OUTPUT@', '-s', '@INPUT@' ])

demo_02_cube_textures_asa = custom_target('demo_02_cube_textures',
  output      : 'demo_02_cube_textures.asa',
  input       : demo_02_cube_textures,
  command     : [ asarc, '-o', '@OUTPUT@', '-t-allow-bc7', 'on', '-t', '@INPUT@' ])

custom_target('demo_02_cube_resources',
  output      : 'demo_02_cube_resources.asa',
  input       : [ demo_02_cube_shaders_asa, demo_02_cube_textures_asa ],
  command     : [ asarc, '-o', '@OUTPUT@', '-a', '@INPUT@' ],
  install     : true,
  install_dir : 'demos/resources')

executable('02-cube', files('main.cpp'),
  link_with   : lib_alseid,
  install     : true,
  install_dir : 'demos')
