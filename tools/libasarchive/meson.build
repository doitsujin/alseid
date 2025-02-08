lib_asarchive_files = files(
  'archive.cpp',
  'basic.cpp',
  'geometry.cpp',
  'merge.cpp',
  'texture.cpp',
  'shader.cpp',
)

lib_asarchive = static_library('asarchive', lib_asarchive_files, as_stb_image_files,
  link_with : [ lib_alseid ])
