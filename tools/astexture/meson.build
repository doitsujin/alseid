as_texture_files = files(
  'main.cpp',
  'texture.cpp',
)

astexture = executable('astexture', as_texture_files, as_lodepng_files, as_nanojpeg_files,
  cpp_args  : as_lodepng_cpp_args,
  link_with : lib_alseid)
