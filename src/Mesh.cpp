#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#include "Mesh.h"
#include <filesystem>
#include <stdexcept>
#include <unordered_map>
#include <algorithm>

namespace fs = std::filesystem;

std::vector<MeshData> loadOBJ(const std::string& path) {
    fs::path objDir = fs::path(path).parent_path();
    std::string dirStr = objDir.string();
    if (!dirStr.empty() && dirStr.back() != '/' && dirStr.back() != '\\') {
        dirStr += "/";
    }

    tinyobj::ObjReaderConfig cfg;
    cfg.mtl_search_path = dirStr; // Позволяем tinyobjloader самому искать mtl файлы
    cfg.triangulate = true;
    tinyobj::ObjReader reader;

    if (!reader.ParseFromFile(path, cfg)) {
        throw std::runtime_error("");
    }

    auto& attrib = reader.GetAttrib();
    auto& shapes = reader.GetShapes();
    auto& materials = reader.GetMaterials();

    std::unordered_map<int, MeshData> groups;

    for (auto& shape : shapes) {
        size_t offset = 0;
        for (size_t fi = 0; fi < shape.mesh.num_face_vertices.size(); fi++) {
            int fv = shape.mesh.num_face_vertices[fi];
            int matId = shape.mesh.material_ids.empty() ? -1 : shape.mesh.material_ids[fi];
            MeshData& md = groups[matId];

            if (matId >= 0 && matId < (int)materials.size()) {
                auto& mat = materials[matId];
                if (md.texturePath.empty() && !mat.diffuse_texname.empty()) {
                    std::string tex = mat.diffuse_texname;
                    std::replace(tex.begin(), tex.end(), '\\', '/'); // Исправление Windows путей
                    md.texturePath = dirStr + tex;
                }
                md.material.diffuse = {mat.diffuse[0], mat.diffuse[1], mat.diffuse[2], 1.f};
                md.material.specular = {mat.specular[0], mat.specular[1], mat.specular[2], 1.f};
                md.material.ambient = {mat.ambient[0], mat.ambient[1], mat.ambient[2], 1.f};
                md.material.shininess = mat.shininess;
            } else {
                md.material.diffuse = {1.f, 1.f, 1.f, 1.f};
                md.material.specular = {1.f, 1.f, 1.f, 1.f};
                md.material.ambient = {0.1f, 0.1f, 0.1f, 1.f};
                md.material.shininess = 32.f;
            }

            for (int v = 0; v < fv; v++) {
                auto idx = shape.mesh.indices[offset + v];
                Vertex vert{};
                vert.pos = {
                    attrib.vertices[3 * idx.vertex_index + 0],
                    attrib.vertices[3 * idx.vertex_index + 1],
                    attrib.vertices[3 * idx.vertex_index + 2],
                };

                if (idx.normal_index >= 0) {
                    vert.normal = {
                        attrib.normals[3 * idx.normal_index + 0],
                        attrib.normals[3 * idx.normal_index + 1],
                        attrib.normals[3 * idx.normal_index + 2],
                    };
                } else {
                    vert.normal = {0.0f, 1.0f, 0.0f}; // Дефолтная нормаль
                }

                if (idx.texcoord_index >= 0) {
                    vert.uv = {
                        attrib.texcoords[2 * idx.texcoord_index + 0],
                        attrib.texcoords[2 * idx.texcoord_index + 1]
                    };
                }

                md.indices.push_back(md.vertices.size());
                md.vertices.push_back(vert);
            }
            offset += fv;
        }
    }

    std::vector<MeshData> out;
    for (auto& [id, md] : groups) {
        if (md.vertices.empty() || md.indices.empty()) continue;
        out.push_back(std::move(md));
    }
    return out;
}
