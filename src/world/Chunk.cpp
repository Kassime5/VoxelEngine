//
// Created by maxim on 03/01/2026.
//

#include "Chunk.h"
#include "World.h"

// position (x, y, z), texCoords (u, v)
static const float FACE_VERTICES[6][4][5] = {
    // Front face (Z+)
    {
        {0.0f, 0.0f, 1.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 1.0f, 1.0f, 0.0f},
        {1.0f, 1.0f, 1.0f, 1.0f, 1.0f},
        {0.0f, 1.0f, 1.0f, 0.0f, 1.0f}
    },
    // Back face (Z-)
    {
        {1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f, 1.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 1.0f, 1.0f},
        {1.0f, 1.0f, 0.0f, 0.0f, 1.0f}
    },
    // Top face (Y+)
    {
        {0.0f, 1.0f, 1.0f, 0.0f, 1.0f},
        {1.0f, 1.0f, 1.0f, 1.0f, 1.0f},
        {1.0f, 1.0f, 0.0f, 1.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f, 0.0f}
    },
    // Bottom face (Y-)
    {
        {0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
        {1.0f, 0.0f, 0.0f, 1.0f, 1.0f},
        {1.0f, 0.0f, 1.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f, 0.0f}
    },
    // Right face (X+)
    {
        {1.0f, 0.0f, 1.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 0.0f, 1.0f, 0.0f},
        {1.0f, 1.0f, 0.0f, 1.0f, 1.0f},
        {1.0f, 1.0f, 1.0f, 0.0f, 1.0f}
    },
    // Left face (X-)
    {
        {0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 1.0f, 0.0f},
        {0.0f, 1.0f, 1.0f, 1.0f, 1.0f},
        {0.0f, 1.0f, 0.0f, 0.0f, 1.0f}
    }
};

// Face normals for culling checks (dx, dy, dz)
static const int FACE_NORMALS[6][3] = {
    {0, 0, 1}, // Front
    {0, 0, -1}, // Back
    {0, 1, 0}, // Top
    {0, -1, 0}, // Bottom
    {1, 0, 0}, // Right
    {-1, 0, 0} // Left
};

Chunk::Chunk(const glm::ivec3 &position)
    : m_position(position), m_isDirty(true) {
    // Initialize all blocks to air
    for (int x = 0; x < SIZE; x++) {
        for (int y = 0; y < HEIGHT; y++) {
            for (int z = 0; z < SIZE; z++) {
                m_blocks[x][y][z] = BlockType::Air;
            }
        }
    }
}

BlockType Chunk::getBlock(int x, int y, int z) const {
    if (x < 0 || x >= SIZE || y < 0 || y >= HEIGHT || z < 0 || z >= SIZE) {
        return BlockType::Air;
    }
    return m_blocks[x][y][z];
}

void Chunk::setBlock(int x, int y, int z, BlockType type) {
    if (x < 0 || x >= SIZE || y < 0 || y >= HEIGHT || z < 0 || z >= SIZE) {
        return;
    }
    m_blocks[x][y][z] = type;
    m_isDirty = true;
}

void Chunk::generate() {
    // TODO: Change world gen
    for (int x = 0; x < SIZE; x++) {
        for (int z = 0; z < SIZE; z++) {
            // Calculate world position
            int worldX = m_position.x * SIZE + x;
            int worldZ = m_position.z * SIZE + z;

            //+ 8.0f * sin(worldX * 0.1f) * cos(worldZ * 0.1f)
            float height = 32.0f + 8.0f * sin(worldX * 0.1f) * cos(worldZ * 0.1f);
            int terrainHeight = static_cast<int>(height);

            for (int y = 0; y < HEIGHT; y++) {
                if (y < terrainHeight - 3) {
                    m_blocks[x][y][z] = BlockType::Stone;
                } else if (y < terrainHeight - 1) {
                    m_blocks[x][y][z] = BlockType::Dirt;
                } else if (y < terrainHeight) {
                    m_blocks[x][y][z] = BlockType::Grass;
                } else {
                    m_blocks[x][y][z] = BlockType::Air;
                }
            }
        }
    }
    m_isDirty = true;
}

bool Chunk::isBlockAt(int x, int y, int z) const {
    if (x < 0 || x >= SIZE || y < 0 || y >= HEIGHT || z < 0 || z >= SIZE) {
        return false; // Treat out-of-bounds as air
    }
    return isBlockOpaque(m_blocks[x][y][z]);
}

bool Chunk::shouldRenderFace(int x, int y, int z, int nx, int ny, int nz, const World *world) const {
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

    if (world) {
        int worldX = m_position.x * SIZE + neighborX;
        int worldY = neighborY;
        int worldZ = m_position.z * SIZE + neighborZ;

        BlockType neighborBlock = world->getBlock(worldX, worldY, worldZ);

        // if (neighborBlock != BlockType::Air) {
        //     if (neighborX < 0 || neighborX >= SIZE || neighborZ < 0 || neighborZ >= SIZE) {
        //         std::cout << "Chunk " << m_position.x << "," << m_position.z
        //                   << " checking (" << worldX << "," << worldY << "," << worldZ << ") = "
        //                   << printBlockType(neighborBlock) << std::endl;
        //     }
        // }

        return !isBlockOpaque(neighborBlock);
    }

    return true;
}

void Chunk::addFace(std::vector<Vertex> &vertices, std::vector<unsigned int> &indices,
                    const glm::vec3 &pos, int faceIndex, BlockType blockType,
                    const TextureAtlas *atlas) {
    unsigned int startIndex = vertices.size();

    std::array<glm::vec2, 4> uvs;
    if (atlas) {
        BlockFace face = static_cast<BlockFace>(faceIndex);
        uvs = atlas->getBlockFaceUVs(blockType, face);
    } else {
        uvs = {glm::vec2(0, 0), glm::vec2(1, 0), glm::vec2(1, 1), glm::vec2(0, 1)};
    }

    for (int i = 0; i < 4; i++) {
        Vertex vertex;
        vertex.position = glm::vec3(
            pos.x + FACE_VERTICES[faceIndex][i][0],
            pos.y + FACE_VERTICES[faceIndex][i][1],
            pos.z + FACE_VERTICES[faceIndex][i][2]
        );
        vertex.texCoords = uvs[i];
        vertex.normal = glm::vec3(FACE_NORMALS[faceIndex][0], FACE_NORMALS[faceIndex][1], FACE_NORMALS[faceIndex][2]);
        vertices.push_back(vertex);
    }

    // Add 2 triangles (6 indices)
    indices.push_back(startIndex + 0);
    indices.push_back(startIndex + 1);
    indices.push_back(startIndex + 2);

    indices.push_back(startIndex + 2);
    indices.push_back(startIndex + 3);
    indices.push_back(startIndex + 0);
}

void Chunk::generateMesh(const TextureAtlas *atlas, const World *world) {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    vertices.reserve(SIZE * SIZE * HEIGHT * 4);
    indices.reserve(SIZE * SIZE * HEIGHT * 6);

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
                        addFace(vertices, indices, blockPos, face, block, atlas);
                    }
                }
            }
        }
    }

    m_mesh.setData(vertices, indices);
    m_isDirty = false;
}

void Chunk::draw() const {
    if (!m_mesh.isEmpty()) {
        m_mesh.draw();
    }
}
