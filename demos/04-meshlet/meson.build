demo_04_shaders = [
  [ 'shaders/object.glsl',    [ 'ms', 'fs' ] ],
  [ 'shaders/task.glsl',      [ 'ts' ] ],
  [ 'shaders/animate.glsl',   [ 'cs' ] ],
  [ 'shaders/transform.glsl', [ 'cs' ] ],
]

demo_04_meshlet_shader_includes = files(
  'shaders/pass.glsl'
)

demo_04_meshlet_spv = []

foreach s : demo_04_shaders
  shader_file = s[0]
  shader_types = s[1]

  foreach p : shader_types
    demo_04_meshlet_spv += custom_target('@0@_@1@'.format(p, shader_file.underscorify()),
      command       : [ glsl_compiler, as_shader_type_args[p] ],
      depend_files  : [ as_shader_includes, demo_04_meshlet_shader_includes ],
      input         : files(shader_file),
      output        : '@0@_@BASENAME@.spv'.format(p))
  endforeach
endforeach

if (spv_opt.found())
#  demo_04_meshlet_spv = spv_optimizer.process(demo_04_meshlet_spv)
endif

demo_04_meshlet_glb = files(
  'resources/CesiumMan.gltf'
)

demo_04_meshlet_geometry_layout = '{"name":"default","attributes":['
demo_04_meshlet_geometry_layout+= '{"name":"POSITION","format":"rgba16f","stream":"vertex","morph":true},'
demo_04_meshlet_geometry_layout+= '{"name":"NORMAL","format":"rgb10a2sn","stream":"shading","morph":true},'
demo_04_meshlet_geometry_layout+= '{"name":"TEXCOORD_0","format":"rg16un","stream":"shading"}]}'

demo_04_meshlet_shaders_asa = custom_target('demo_04_meshlet_shaders',
  output      : 'demo_04_meshlet_shaders.asa',
  input       : demo_04_meshlet_spv,
  command     : [ asarc, '-o', '@OUTPUT@', '-s', '@INPUT@' ])

demo_04_meshlet_geometry_asa = custom_target('demo_04_meshlet_geometry',
  output      : 'demo_04_meshlet_geometry.asa',
  input       : demo_04_meshlet_glb,
  command     : [ asarc, '-o', '@OUTPUT@', '-g-layout',
    demo_04_meshlet_geometry_layout, '-g', '@INPUT@' ])

custom_target('demo_04_meshlet_resources',
  output      : 'demo_04_meshlet_resources.asa',
  input       : [ demo_04_meshlet_shaders_asa, demo_04_meshlet_geometry_asa ],
  command     : [ asarc, '-o', '@OUTPUT@', '-a', '@INPUT@' ],
  install     : true,
  install_dir : 'demos/resources')

executable('04-meshlet', files('main.cpp'),
  link_with   : lib_alseid,
  install     : true,
  install_dir : 'demos')
