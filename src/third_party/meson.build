as_files += files([
  'bc7enc/bc7enc.cpp',
  'bc7enc/rgbcx.cpp',

  'sha1/sha1.c',
])

as_lodepng_files = files([
  'lodepng/lodepng.cpp',
])

as_lodepng_cpp_args = [
  '-DLODEPNG_NO_COMPILE_ENCODER=1',
  '-DLODEPNG_NO_COMPILE_DISK=1'
]

as_nanojpeg_files = files([
  'nanojpeg/nanojpeg.c',
])

as_gdeflate_glsl = custom_target('cs_gdeflate',
  output  : 'cs_gdeflate.h',
  input   : files('gdeflate/gdeflate.glsl'),
  command : [ glsl_compiler, '--target-env', 'vulkan1.3', '--vn', 'cs_gdeflate', '-S', 'comp', '-o', '@OUTPUT@', '@INPUT@' ])

as_files += as_gdeflate_glsl

libdeflate_files = files([
  'libdeflate/lib/x86/cpu_features.c',

  'libdeflate/lib/adler32.c',
  'libdeflate/lib/crc32.c',
  'libdeflate/lib/deflate_compress.c',
  'libdeflate/lib/deflate_decompress.c',
  'libdeflate/lib/gdeflate_compress.c',
  'libdeflate/lib/gdeflate_decompress.c',
  'libdeflate/lib/gzip_compress.c',
  'libdeflate/lib/gzip_decompress.c',
  'libdeflate/lib/utils.c',
  'libdeflate/lib/zlib_compress.c',
  'libdeflate/lib/zlib_decompress.c',
])

libdeflate_lib = static_library('libdeflate', libdeflate_files,
  include_directories : 'libdeflate')

libdeflate_dep = declare_dependency(
  link_with           : libdeflate_lib,
  include_directories : 'libdeflate')

as_dependencies += libdeflate_dep
