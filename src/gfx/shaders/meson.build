as_shader_includes = files(
  'as_animation.glsl',
  'as_common.glsl',
  'as_geometry.glsl',
  'as_include_head.glsl',
  'as_include_tail.glsl',
  'as_matrix.glsl',
  'as_quaternion.glsl',
  'cs_template.glsl',
  'fs_template.glsl',
  'ms_common.glsl',
  'ms_template.glsl',
  'ts_common.glsl',
  'ts_template.glsl',

  'render/ms_render_common.glsl',
  'render/ms_render_instance.glsl',
  'render/ts_render_common.glsl',
  'render/ts_render_instance.glsl',
  'render/ts_render_payload.glsl',

  'scene/as_pass.glsl',
  'scene/as_scene.glsl',
  'scene/as_scene_draw.glsl',
  'scene/as_scene_instance.glsl',
  'scene/as_scene_node_transform.glsl',
  'scene/as_scene_stream.glsl',
)

as_shader_builtins = [
  [ 'scene/cs_draw_list_init.comp', 'cs' ],
  [ 'scene/cs_draw_list_generate.comp', 'cs' ],

  [ 'scene/cs_instance_animate.comp', 'cs' ],
  [ 'scene/cs_instance_animate_prepare.comp', 'cs' ],
  [ 'scene/cs_instance_update_execute.comp', 'cs' ],
  [ 'scene/cs_instance_update_prepare.comp', 'cs' ],

  [ 'scene/cs_pass_info_update_execute.comp', 'cs' ],
  [ 'scene/cs_pass_info_update_copy.comp', 'cs' ],
  [ 'scene/cs_pass_info_update_init.comp', 'cs' ],
  [ 'scene/cs_pass_info_update_prepare.comp', 'cs' ],

  [ 'scene/cs_pass_init.comp', 'cs' ],
  [ 'scene/cs_pass_reset_update.comp', 'cs' ],
  [ 'scene/cs_pass_traverse_bvh.comp', 'cs' ],
  [ 'scene/cs_pass_traverse_reset.comp', 'cs' ],

  [ 'scene/cs_scene_update.comp', 'cs' ],
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
  as_files += custom_target('@0@.h'.format(s[0].underscorify()),
    command       : [ glsl_compiler, as_shader_type_args[s[1]], as_shader_builtin_args ],
    depend_files  : [ as_shader_includes ],
    input         : files(s[0]),
    output        : '@BASENAME@.h')
endforeach
