as_files = files([
  'gfx/gfx.cpp',
  'gfx/gfx_buffer.cpp',
  'gfx/gfx_format.cpp',
  'gfx/gfx_image.cpp',
  'gfx/gfx_pipeline.cpp',
  'gfx/gfx_presenter.cpp',
  'gfx/gfx_scratch.cpp',
  'gfx/gfx_shader.cpp',
  'gfx/gfx_spirv.cpp',
  'gfx/gfx_submission.cpp',

  'gfx/debug/gfx_debug_device.cpp',

  'jobs/job.cpp',
  'jobs/job_manager.cpp',

  'util/util_hash.cpp',
  'util/util_huffman.cpp',
  'util/util_log.cpp',

  'wsi/wsi.cpp',
])

as_defines = []

as_dependencies = [ spirv_cross_core_dep ]

dep_sdl2 = dependency('sdl2',
  required      : get_option('enable-sdl2'))

if get_option('enable-sdl2')
  as_defines += [ 'ALSEID_WSI_SDL2' ]
  as_dependencies += [ dep_sdl2 ]

  subdir('wsi/sdl2')
endif

if get_option('enable-vulkan')
  as_defines += [ 'ALSEID_GFX_VULKAN' ]

  subdir('gfx/vulkan')
endif

foreach def : as_defines
  add_project_arguments('-D' + def,
    language      : 'cpp')
endforeach

subdir('third_party')

lib_alseid = static_library('alseid', as_files,
  dependencies  : as_dependencies)
