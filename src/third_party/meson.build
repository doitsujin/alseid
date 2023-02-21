as_files += files([
  'bc7enc/bc7enc.cpp',
  'bc7enc/rgbcx.cpp',

  'lodepng/lodepng.cpp',

  'nanojpeg/nanojpeg.c',

  'sha1/sha1.c',
])

add_project_arguments([
  '-DLODEPNG_NO_COMPILE_ENCODER=1',
  '-DLODEPNG_NO_COMPILE_DISK=1'
], language : 'cpp')
