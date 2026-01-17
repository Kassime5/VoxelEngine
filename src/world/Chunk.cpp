//
// Created by maxim on 03/01/2026.
//

#include "Chunk.h"
#include <cstring>
#include "World.h"

// position (x, y, z), texCoords (u, v)
static const uint8_t FACE_VERTEX_OFFSETS[6][4][3] = {
    // Front face (Z+)
    {{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}},
    // Back face (Z-)
    {{1, 0, 0}, {0, 0, 0}, {0, 1, 0}, {1, 1, 0}},
    // Top face (Y+)
    {{0, 1, 1}, {1, 1, 1}, {1, 1, 0}, {0, 1, 0}},
    // Bottom face (Y-)
    {{0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1}},
    // Right face (X+)
    {{1, 0, 1}, {1, 0, 0}, {1, 1, 0}, {1, 1, 1}},
    // Left face (X-)
    {{0, 0, 0}, {0, 0, 1}, {0, 1, 1}, {0, 1, 0}}
};

static const int FACE_NORMALS[6][3] = {
    {0, 0, 1}, // Front
    {0, 0, -1}, // Back
    {0, 1, 0}, // Top
    {0, -1, 0}, // Bottom
    {1, 0, 0}, // Right
    {-1, 0, 0} // Left
};

Chunk::Chunk(const glm::ivec3 &position)
    : m_position(position), m_isDirty(true), m_state(ChunkState::Empty) {
    for (auto & m_block : m_blocks) {
        for (auto & y : m_block) {
            for (auto & z : y) {
                z = BlockType::Air;
            }
        }
    }
}

BlockType Chunk::getBlock(int x, int y, int z) const {
    if (x < 0 || x >= SIZE || y < 0 || y >= HEIGHT || z < 0 || z >= SIZE) {
        return BlockType::Air;
    }
    std::lock_guard<std::mutex> lock(m_blocksMutex);
    return m_blocks[x][y][z];
}

void Chunk::setBlock(int x, int y, int z, BlockType type) {
    if (x < 0 || x >= SIZE || y < 0 || y >= HEIGHT || z < 0 || z >= SIZE) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_blocksMutex);
    m_blocks[x][y][z] = type;
    m_isDirty = true;
}

void Chunk::generate(const siv::PerlinNoise* perlinNoise) {
    // std::lock_guard<std::mutex> lock(m_blocksMutex);
    for (int x = 0; x < SIZE; x++) {
        for (int z = 0; z < SIZE; z++) {
            int worldX = m_position.x * SIZE + x;
            int worldZ = m_position.z * SIZE + z;

            double noise = perlinNoise->octave2D_01(worldX * 0.01, worldZ * 0.01, 4);
            float height = 16.0f + static_cast<float>(noise) * 32.0f;
            int terrainHeight = static_cast<int>(height);

            for (int y = 0; y < HEIGHT; y++) {
                if (y < terrainHeight - 3) {
                    m_blocks[x][y][z] = BlockType::Stone;
                } else if (y < terrainHeight - 1) {
                    m_blocks[x][y][z] = BlockType::Dirt;
                } else if (y < terrainHeight) {
                    m_blocks[x][y][z] = BlockType::Grass;
                }
            }
        }
    }
    m_isDirty = true;
    m_state.store(ChunkState::Generated);
}

void Chunk::buildMeshData(MeshData& meshData, const TextureAtlas *atlas, World *world) {
    PROFILE_SCOPE("Chunk::buildMeshData");

    meshData.clear();
    meshData.vertices.reserve(SIZE * SIZE * 4);
    meshData.indices.reserve(SIZE * SIZE * 6);

    for (int x = 0; x < SIZE; x++) {
        for (int y = 0; y < HEIGHT; y++) {
            for (int z = 0; z < SIZE; z++) {
                BlockType block = m_blocks[x][y][z];
                if (block == BlockType::Air) continue;

                glm::vec3 blockPos(x, y, z);

                for (int face = 0; face < 6; face++) {
                    int nx = FACE_NORMALS[face][0];
                    int ny = FACE_NORMALS[face][1];
                    int nz = FACE_NORMALS[face][2];

                    if (shouldRenderFace(x, y, z, nx, ny, nz, world)) {
                        addFace(meshData.vertices, meshData.indices, blockPos, face, block, atlas);
                    }
                }
            }
        }
    }

    m_state.store(ChunkState::MeshBuilt);
}

void Chunk::uploadMeshToGPU(const MeshData& meshData) {
    PROFILE_SCOPE("Chunk::uploadMeshToGPU");
    m_mesh.setData(meshData.vertices, meshData.indices);
    m_isDirty = false;
    m_state.store(ChunkState::Ready);
}

bool Chunk::isBlockAt(int x, int y, int z) const {
    if (x < 0 || x >= SIZE || y < 0 || y >= HEIGHT || z < 0 || z >= SIZE) {
        return false; // Treat out-of-bounds as air
    }
    return isBlockOpaque(m_blocks[x][y][z]);
}

bool Chunk::shouldRenderFace(int x, int y, int z, int nx, int ny, int nz, World *world) const {
    int neighborX = x + nx;
    int neighborY = y + ny;
    int neighborZ = z + nz;

    // Never render the bottom of the map
    if (neighborY == -1)
        return false;

    // Check if neighbor is within this chunk
    if (neighborX >= 0 && neighborX < SIZE && neighborY >= 0 && neighborY < HEIGHT && neighborZ >= 0 && neighborZ <
        SIZE) {
        return !isBlockOpaque(m_blocks[neighborX][neighborY][neighborZ]);
    }

    // TODO: Currently doesn't work if the chunk is still generating
    if (world) {
        int worldX = m_position.x * SIZE + neighborX;
        int worldY = neighborY;
        int worldZ = m_position.z * SIZE + neighborZ;
        BlockType neighborBlock = world->getBlock(worldX, worldY, worldZ);
        return !isBlockOpaque(neighborBlock);
    }

    return true;
}

void Chunk::addFace(std::vector<Vertex> &vertices, std::vector<unsigned int> &indices,
                    const glm::vec3 &pos, int faceIndex, BlockType blockType,
                    const TextureAtlas *atlas) {
    unsigned int startIndex = vertices.size();

    std::array<glm::vec2, 4> uvs;
    uint8_t tileIndex = 0;
    if (atlas) {
        BlockFace face = static_cast<BlockFace>(faceIndex);
        tileIndex = atlas->getBlockFaceTileIndex(blockType, face);
    }

    for (int i = 0; i < 4; i++) {
        Vertex vertex;
        vertex.position = glm::u8vec3(
            static_cast<uint8_t>(pos.x) + FACE_VERTEX_OFFSETS[faceIndex][i][0],
            static_cast<uint8_t>(pos.y) + FACE_VERTEX_OFFSETS[faceIndex][i][1],
            static_cast<uint8_t>(pos.z) + FACE_VERTEX_OFFSETS[faceIndex][i][2]
        );
        vertex.tileIndex = tileIndex;
        vertex.cornerIndex = i;  // 0, 1, 2, 3 for the four corners
        vertex.normalId = faceIndex;
        vertices.push_back(vertex);
    }

    indices.push_back(startIndex + 0);
    indices.push_back(startIndex + 1);
    indices.push_back(startIndex + 2);

    indices.push_back(startIndex + 2);
    indices.push_back(startIndex + 3);
    indices.push_back(startIndex + 0);
}

void Chunk::draw() const {
    if (!m_mesh.isEmpty()) {
        m_mesh.draw();
    }
}
