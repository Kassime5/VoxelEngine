//
// Created by maxim on 17/02/2026.
//

#include "Model.h"

#include <array>
#include <fstream>
#include <map>
#include <sstream>
#include <unordered_map>

unsigned int TextureFromFile(const char* path, const std::string& directory)
{
    std::string filename = std::string(path);
    filename = directory + '/' + filename;

    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char* data = stbi_load(filename.c_str(), &width, &height, &nrComponents, 0);
    if (data)
    {
        GLenum format;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        stbi_image_free(data);
    }
    else
    {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}

void Model::Draw(Shader& shader)
{
    for (unsigned int i = 0; i < meshes.size(); i++)
        meshes[i].Draw(shader);
}

namespace {

// One corner of an OBJ face, resolved to 0-based indices into the position / uv / normal
// arrays. -1 means the field was absent from the token.
struct Corner {
    int v = -1;
    int vt = -1;
    int vn = -1;
};

// OBJ indices are 1-based, and a negative value counts back from the end of whatever has
// been parsed so far rather than from the end of the file.
int resolveIndex(int raw, std::size_t count) {
    if (raw > 0) {
        return raw - 1;
    }
    if (raw < 0) {
        return static_cast<int>(count) + raw;
    }
    return -1;
}

// Accepts every form the spec allows: "v", "v/vt", "v//vn" and "v/vt/vn".
Corner parseCorner(const std::string& token, std::size_t positions, std::size_t uvs, std::size_t normals) {
    int raw[3] = {0, 0, 0};

    std::size_t start = 0;
    for (int part = 0; part < 3; ++part) {
        const std::size_t slash = token.find('/', start);
        const std::string field = token.substr(
            start, slash == std::string::npos ? std::string::npos : slash - start);

        if (!field.empty()) {
            raw[part] = std::stoi(field);
        }
        if (slash == std::string::npos) {
            break;
        }
        start = slash + 1;
    }

    return { resolveIndex(raw[0], positions),
             resolveIndex(raw[1], uvs),
             resolveIndex(raw[2], normals) };
}

// Material name -> diffuse map filename, read from the .mtl's newmtl / map_Kd pairs.
std::unordered_map<std::string, std::string> parseMaterialLibrary(const std::string& path) {
    std::unordered_map<std::string, std::string> diffuseMaps;

    std::ifstream file(path);
    if (!file) {
        std::cout << "WARNING::MODEL:: could not open material library " << path << std::endl;
        return diffuseMaps;
    }

    std::string line;
    std::string currentMaterial;
    while (std::getline(file, line)) {
        std::istringstream in(line);
        std::string keyword;
        in >> keyword;

        if (keyword == "newmtl") {
            in >> currentMaterial;
        } else if (keyword == "map_Kd" && !currentMaterial.empty()) {
            std::string mapFile;
            in >> mapFile;
            diffuseMaps[currentMaterial] = mapFile;
        }
    }

    return diffuseMaps;
}

} // namespace

void Model::loadModel(const std::string& path)
{
    std::ifstream file(path);
    if (!file) {
        std::cout << "ERROR::MODEL:: failed to open " << path << std::endl;
        return;
    }

    const std::size_t lastSlash = path.find_last_of("/\\");
    directory = lastSlash == std::string::npos ? std::string(".") : path.substr(0, lastSlash);

    // Positions, UVs and normals are indexed file-globally in OBJ -- o/g groups only
    // partition the faces -- so these accumulate across the entire file and are never
    // cleared when a mesh is flushed.
    std::vector<glm::vec3> positions;
    std::vector<glm::vec2> uvs;
    std::vector<glm::vec3> normals;

    std::unordered_map<std::string, std::string> diffuseMaps;
    std::string materialName;

    // Per-mesh accumulators. OBJ indexes position/uv/normal separately, so each distinct
    // combination becomes one GL vertex and `emitted` maps it back to its index.
    std::vector<MeshVertex> vertices;
    std::vector<unsigned int> indices;
    std::map<std::array<int, 3>, unsigned int> emitted;

    auto flushMesh = [&]() {
        if (indices.empty()) {
            return;
        }

        std::vector<MeshTexture> textures;
        const auto diffuse = diffuseMaps.find(materialName);
        if (diffuse != diffuseMaps.end()) {
            textures = loadDiffuseTexture(diffuse->second);
        }

        meshes.emplace_back(vertices, indices, textures);
        vertices.clear();
        indices.clear();
        emitted.clear();
    };

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream in(line);
        std::string keyword;
        in >> keyword;

        if (keyword == "v") {
            glm::vec3 position{};
            in >> position.x >> position.y >> position.z;
            positions.push_back(position);
        } else if (keyword == "vt") {
            glm::vec2 uv{};
            in >> uv.x >> uv.y;
            // Assimp was importing with aiProcess_FlipUVs and the texture is authored for
            // that orientation, so the flip has to be reproduced here.
            uv.y = 1.0f - uv.y;
            uvs.push_back(uv);
        } else if (keyword == "vn") {
            glm::vec3 normal{};
            in >> normal.x >> normal.y >> normal.z;
            normals.push_back(normal);
        } else if (keyword == "f") {
            std::vector<unsigned int> corners;

            std::string token;
            while (in >> token) {
                const Corner corner = parseCorner(token, positions.size(), uvs.size(), normals.size());
                const std::array<int, 3> key{ corner.v, corner.vt, corner.vn };

                const auto [entry, inserted] =
                    emitted.try_emplace(key, static_cast<unsigned int>(vertices.size()));
                if (inserted) {
                    MeshVertex vertex{};
                    if (corner.v >= 0 && corner.v < static_cast<int>(positions.size())) {
                        vertex.Position = positions[corner.v];
                    }
                    if (corner.vn >= 0 && corner.vn < static_cast<int>(normals.size())) {
                        vertex.Normal = normals[corner.vn];
                    }
                    if (corner.vt >= 0 && corner.vt < static_cast<int>(uvs.size())) {
                        vertex.TexCoords = uvs[corner.vt];
                    }
                    vertices.push_back(vertex);
                }

                corners.push_back(entry->second);
            }

            // Fan triangulation, matching what aiProcess_Triangulate did to these quads.
            for (std::size_t i = 1; i + 1 < corners.size(); ++i) {
                indices.push_back(corners[0]);
                indices.push_back(corners[i]);
                indices.push_back(corners[i + 1]);
            }
        } else if (keyword == "o") {
            // Assimp produced one mesh per object and the TODO about tracking the head
            // separately wants that split kept, so groups stay as distinct meshes.
            flushMesh();
        } else if (keyword == "usemtl") {
            in >> materialName;
        } else if (keyword == "mtllib") {
            std::string library;
            in >> library;
            diffuseMaps = parseMaterialLibrary(directory + '/' + library);
        }
    }

    flushMesh();
}

std::vector<MeshTexture> Model::loadDiffuseTexture(const std::string& file)
{
    for (const MeshTexture& loaded : textures_loaded) {
        if (loaded.path == file) {
            return { loaded };
        }
    }

    MeshTexture texture;
    texture.id = TextureFromFile(file.c_str(), directory);
    texture.type = "texture_diffuse";
    texture.path = file;
    textures_loaded.push_back(texture);

    return { texture };
}
