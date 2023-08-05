lib_gltfimport_files = files(
  'gltf_asset.cpp',
  'gltf_import.cpp',
)

lib_gltfimport = static_library('gltfimport', lib_gltfimport_files,
  dependencies  : [ meshoptimizer_dep ],
  link_with     : [ lib_alseid ])
