#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#include "Mesh.h"
#include <filesystem>
#include <stdexcept>
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <glm/gtc/constants.hpp>

namespace fs = std::filesystem;

std::vector<MeshData> loadOBJ(const std::string& path) {
    fs::path objDir = fs::path(path).parent_path();
    std::string dirStr = objDir.string();
    if (!dirStr.empty() && dirStr.back() != '/' && dirStr.back() != '\\') {
        dirStr += "/";
    }

    tinyobj::ObjReaderConfig cfg;
    cfg.mtl_search_path = dirStr;
    cfg.triangulate = true;
    tinyobj::ObjReader reader;

    if (!reader.ParseFromFile(path, cfg)) {
        throw std::runtime_error("Failed to load OBJ: " + reader.Error());
    }

    auto& attrib = reader.GetAttrib();
    auto& shapes = reader.GetShapes();
    auto& materials = reader.GetMaterials();

    std::unordered_map<int, MeshData> groups;
    bool warnedAboutUVs = false;

    auto resolvePath = [&](const std::string& texName) -> std::string {
        if (texName.empty()) return "";
        std::string t = texName;
        t.erase(t.find_last_not_of(" \t\r\n") + 1);
        t.erase(0, t.find_first_not_of(" \t\r\n"));
        std::replace(t.begin(), t.end(), '\\', '/');
        fs::path p(t);
        if (p.is_absolute()) return p.string();
        return (fs::path(dirStr) / p).string();
    };

    for (auto& shape : shapes) {
        size_t offset = 0;
        for (size_t fi = 0; fi < shape.mesh.num_face_vertices.size(); fi++) {
            int fv = shape.mesh.num_face_vertices[fi];
            int matId = shape.mesh.material_ids.empty() ? -1 : shape.mesh.material_ids[fi];
            MeshData& md = groups[matId];

            if (matId >= 0 && matId < (int)materials.size()) {
                auto& mat = materials[matId];

                if (md.diffuseTex.empty()) md.diffuseTex = resolvePath(mat.diffuse_texname);
                if (md.diffuseTex.empty()) md.diffuseTex = resolvePath(mat.ambient_texname); // Запасной вариант (map_Ka)

                if (md.normalTex.empty())  md.normalTex  = resolvePath(mat.bump_texname);
                if (md.normalTex.empty())  md.normalTex  = resolvePath(mat.normal_texname);
                if (md.dispTex.empty())    md.dispTex    = resolvePath(mat.displacement_texname);

                md.material.diffuse = {mat.diffuse[0], mat.diffuse[1], mat.diffuse[2], 1.f};
                md.material.specular = {mat.specular[0], mat.specular[1], mat.specular[2], 1.f};
                md.material.ambient = {mat.ambient[0], mat.ambient[1], mat.ambient[2], 1.f};
                md.material.shininess = mat.shininess;

                // ПРИНУДИТЕЛЬНО делаем материал белым, чтобы текстура не закрашивалась в черный
                if (!md.diffuseTex.empty()) {
                    md.material.diffuse = {1.f, 1.f, 1.f, 1.f};
                }
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
                    vert.normal = {0.0f, 1.0f, 0.0f};
                }

                if (idx.texcoord_index >= 0) {
                    vert.uv = {
                        attrib.texcoords[2 * idx.texcoord_index + 0],
                        1.0f - attrib.texcoords[2 * idx.texcoord_index + 1]
                    };
                } else {
                    if (!warnedAboutUVs) {
                        std::cout << "\n[WARNING] Model has missing UVs! Generating fake spherical UVs.\n";
                        warnedAboutUVs = true;
                    }
                    glm::vec3 n = glm::normalize(vert.normal);
                    vert.uv.x = 0.5f + (atan2(n.z, n.x) / (2.0f * glm::pi<float>()));
                    vert.uv.y = 0.5f - (asin(n.y) / glm::pi<float>());
                }

                vert.tangent = {0.0f, 0.0f, 0.0f};
                md.indices.push_back(md.vertices.size());
                md.vertices.push_back(vert);
            }
            offset += fv;
        }
    }

    std::vector<MeshData> out;
    for (auto& [id, md] : groups) {
        if (md.vertices.empty() || md.indices.empty()) continue;

        if (!md.diffuseTex.empty()) {
            std::cout << "[DEBUG] Preparing Mesh with Diffuse: " << md.diffuseTex << "\n";
        }

        for (size_t i = 0; i < md.indices.size(); i += 3) {
            uint32_t i0 = md.indices[i]; uint32_t i1 = md.indices[i+1]; uint32_t i2 = md.indices[i+2];
            Vertex& v0 = md.vertices[i0]; Vertex& v1 = md.vertices[i1]; Vertex& v2 = md.vertices[i2];

            glm::vec3 edge1 = v1.pos - v0.pos; glm::vec3 edge2 = v2.pos - v0.pos;
            glm::vec2 deltaUV1 = v1.uv - v0.uv; glm::vec2 deltaUV2 = v2.uv - v0.uv;

            float det = deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y;
            glm::vec3 tangent(0.0f);

            if (std::abs(det) > 1e-6f) {
                float f = 1.0f / det;
                tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
                tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
                tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);
            }

            v0.tangent += tangent; v1.tangent += tangent; v2.tangent += tangent;
        }

        for (auto& v : md.vertices) {
            glm::vec3 t = v.tangent - v.normal * glm::dot(v.normal, v.tangent);
            if (glm::length(t) > 1e-6f) {
                v.tangent = glm::normalize(t);
            } else {
                glm::vec3 c1 = glm::cross(v.normal, glm::vec3(0.0, 0.0, 1.0));
                glm::vec3 c2 = glm::cross(v.normal, glm::vec3(0.0, 1.0, 0.0));
                v.tangent = glm::length(c1) > glm::length(c2) ? glm::normalize(c1) : glm::normalize(c2);
            }
        }
        out.push_back(std::move(md));
    }
    return out;
}
