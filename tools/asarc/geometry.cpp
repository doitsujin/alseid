#include "geometry.h"

struct GeometryFile {
  std::string name;
  std::string input;
};


void from_json(const json& j, GeometryFile& args) {
  if (j.count("name"))
    j.at("name").get_to(args.name);

  if (j.count("input"))
    j.at("input").get_to(args.input);
}


void processGeometries(ArchiveBuilder& builder, const json& j) {
  std::vector<GltfPackedVertexLayoutDesc> layouts;
  std::vector<GeometryFile> files;

  if (j.count("geometry_layouts"))
    j.at("geometry_layouts").get_to(layouts);

  if (j.count("geometries"))
    j.at("geometries").get_to(files);

  GeometryDesc geometryDesc;
  geometryDesc.layoutMap = std::make_shared<GltfPackedVertexLayoutMap>();

  for (const auto& layout : layouts)
    geometryDesc.layoutMap->emplace(layout);

  for (const auto& geometry : files) {
    geometryDesc.name = geometry.name;
    builder.addBuildJob(std::make_shared<GeometryBuildJob>(g_env, geometryDesc, g_basedir / geometry.input));
  }
}
