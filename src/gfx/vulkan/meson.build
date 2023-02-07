as_files += files([
  'gfx_vulkan.cpp',
  'gfx_vulkan_adapter.cpp',
  'gfx_vulkan_barrier.cpp',
  'gfx_vulkan_buffer.cpp',
  'gfx_vulkan_command_list.cpp',
  'gfx_vulkan_context.cpp',
  'gfx_vulkan_descriptor_array.cpp',
  'gfx_vulkan_descriptor_pool.cpp',
  'gfx_vulkan_device.cpp',
  'gfx_vulkan_format.cpp',
  'gfx_vulkan_image.cpp',
  'gfx_vulkan_loader.cpp',
  'gfx_vulkan_memory.cpp',
  'gfx_vulkan_pipeline.cpp',
  'gfx_vulkan_presenter.cpp',
  'gfx_vulkan_queue.cpp',
  'gfx_vulkan_sampler.cpp',
  'gfx_vulkan_semaphore.cpp',
  'gfx_vulkan_utils.cpp',

  'wsi/gfx_vulkan_wsi.cpp',
  'wsi/gfx_vulkan_wsi_null.cpp',
])

as_vk_shaders = files([
  'shaders/cs_present_blit.comp',
  'shaders/fs_present_blit.frag',
  'shaders/vs_present_blit.vert',
])

as_files += glsl_generator.process(as_vk_shaders)

if get_option('enable-sdl2')
  as_files += files(['wsi/gfx_vulkan_wsi_sdl2.cpp'])
endif
