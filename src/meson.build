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
  'gfx/gfx_transfer.cpp',

  'gfx/debug/gfx_debug_device.cpp',

  'io/io.cpp',
  'io/io_archive.cpp',
  'io/io_request.cpp',
  'io/io_stream.cpp',

  'job/job.cpp',

  'util/util_deflate.cpp',
  'util/util_hash.cpp',
  'util/util_log.cpp',
  'util/util_stream.cpp',

  'wsi/wsi.cpp',
])

as_defines = []

as_dependencies = [ spirv_cross_core_dep ]

if get_option('enable-sdl2')
  dep_sdl2 = dependency('sdl2',
    required : true)

  as_defines += [ 'ALSEID_WSI_SDL2' ]
  as_dependencies += [ dep_sdl2 ]

  subdir('wsi/sdl2')
endif

if get_option('enable-vulkan')
  as_defines += [ 'ALSEID_GFX_VULKAN' ]

  subdir('gfx/vulkan')
endif

subdir('io/stl')

liburing = cpp_compiler.find_library('uring',
  required : get_option('enable-liburing'))

if liburing.found()
  as_defines += [ 'ALSEID_IO_URING' ]
  as_dependencies += [ liburing ]

  subdir('io/uring')
endif

foreach def : as_defines
  add_project_arguments('-D' + def,
    language      : 'cpp')
endforeach

subdir('third_party')

lib_alseid = static_library('alseid', as_files,
  dependencies  : as_dependencies)
