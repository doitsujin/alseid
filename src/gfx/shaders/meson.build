as_shader_includes = files(
  'as_animation.glsl',
  'as_common.glsl',
  'as_geometry.glsl',
  'as_include_head.glsl',
  'as_include_tail.glsl',
  'as_matrix.glsl',
  'as_quaternion.glsl',
  'cs_joint_animation.glsl',
  'cs_joint_transform.glsl',
  'cs_template.glsl',
  'fs_template.glsl',
  'ms_common.glsl',
  'ms_template.glsl',
  'ts_common.glsl',
  'ts_template.glsl',
  'vs_template.glsl',

  'scene/as_pass.glsl',
  'scene/as_scene.glsl',
  'scene/as_scene_draw.glsl',
  'scene/as_scene_node_transform.glsl',
  'scene/as_scene_stream.glsl',
)

as_shader_builtins = [
  'scene/cs_pass_init.comp',
  'scene/cs_pass_traverse_bvh.comp',
  'scene/cs_pass_traverse_reset.comp',

  'scene/cs_scene_update.comp',
]

as_shader_glslang_args = [
  '-I' + meson.current_source_dir(),
  '--target-env', 'vulkan1.3',
  '-o', '@OUTPUT@', '@INPUT@' ]

as_shader_builtin_args = [ '--vn', '@BASENAME@' ]

as_shader_type_args = {
  'cs'  : [ '-S', 'comp', '-DSTAGE_COMP' ] + as_shader_glslang_args,
  'vs'  : [ '-S', 'vert', '-DSTAGE_VERT' ] + as_shader_glslang_args,
  'ts'  : [ '-S', 'task', '-DSTAGE_TASK' ] + as_shader_glslang_args,
  'ms'  : [ '-S', 'mesh', '-DSTAGE_MESH' ] + as_shader_glslang_args,
  'fs'  : [ '-S', 'frag', '-DSTAGE_FRAG' ] + as_shader_glslang_args,
}

foreach s : as_shader_builtins
  as_files += custom_target('@0@.h'.format(s.underscorify()),
    command       : [ glsl_compiler, as_shader_type_args['cs'], as_shader_builtin_args ],
    depend_files  : [ as_shader_includes ],
    input         : files(s),
    output        : '@BASENAME@.h')
endforeach
