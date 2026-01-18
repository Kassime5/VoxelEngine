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
    : chunkPosition(position), chunkDirty(true), chunkState(ChunkState::Empty) {
    for (auto & m_block : chunkBlocks) {
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
    return chunkBlocks[x][y][z];
}

void Chunk::setBlock(int x, int y, int z, BlockType type) {
    if (x < 0 || x >= SIZE || y < 0 || y >= HEIGHT || z < 0 || z >= SIZE) {
        return;
    }
    chunkBlocks[x][y][z] = type;
    chunkDirty = true;
}

void Chunk::generate(const siv::PerlinNoise* perlinNoise, const WorleyBiome* worleyGenerator) {
    for (int x = 0; x < SIZE; x++) {
        for (int z = 0; z < SIZE; z++) {
            int worldX = chunkPosition.x * SIZE + x;
            int worldZ = chunkPosition.z * SIZE + z;

            float height = worleyGenerator->getBlendedHeight(worldX, worldZ, perlinNoise);
            int terrainHeight = static_cast<int>(height);

            // Get the biome config
            const BiomeConfig& closestConfig = worleyGenerator->getConfigAt(worldX, worldZ);

            for (int y = 0; y < HEIGHT; y++) {
                if (y < terrainHeight - 3) {
                    chunkBlocks[x][y][z] = closestConfig.stoneBlock;
                } else if (y < terrainHeight - 1) {
                    chunkBlocks[x][y][z] = closestConfig.subSurfaceBlock;
                } else if (y < terrainHeight) {
                    chunkBlocks[x][y][z] = closestConfig.surfaceBlock;
                } else {
                    chunkBlocks[x][y][z] = BlockType::Air;
                }
            }
        }
    }
    chunkDirty = true;
    chunkState.store(ChunkState::Generated);
}

void Chunk::buildMeshData(MeshData& meshData, const TextureAtlas *atlas, World *world) {
    PROFILE_SCOPE("Chunk::buildMeshData");

    meshData.clear();
    meshData.vertices.reserve(SIZE * SIZE * 4);
    meshData.indices.reserve(SIZE * SIZE * 6);

    // Greedy mesh each axis separately
    greedyMeshAxis(meshData, atlas, world, 0); // X-axis (YZ plane)
    greedyMeshAxis(meshData, atlas, world, 1); // Y-axis (XZ plane)
    greedyMeshAxis(meshData, atlas, world, 2); // Z-axis (XY plane)

    chunkState.store(ChunkState::MeshBuilt);
}

void Chunk::greedyMeshAxis(MeshData& meshData, const TextureAtlas *atlas, World *world, int axis) {
    // axis: 0=X, 1=Y, 2=Z
    // We'll scan along this axis and mesh the perpendicular plane
    int u = (axis + 1) % 3; // First perpendicular axis
    int v = (axis + 2) % 3; // Second perpendicular axis

    int chunkSize[3] = {SIZE, HEIGHT, SIZE};
    int x[3] = {0, 0, 0};
    int q[3] = {0, 0, 0};
    q[axis] = 1; // Direction vector along the axis

    // Mapping from axis to face directions
    // axis 0 (X): faces 0 (right) and 1 (left)
    // axis 1 (Y): faces 2 (top) and 3 (bottom)
    // axis 2 (Z): faces 4 (front) and 5 (back)
    BlockFace positiveFace = static_cast<BlockFace>(axis * 2);
    BlockFace negativeFace = static_cast<BlockFace>(axis * 2 + 1);

    // For each slice along the axis
    for (x[axis] = -1; x[axis] < chunkSize[axis]; ) {
        // Build mask for this slice
        struct MaskEntry {
            BlockType blockType;
            BlockFace face;
        };

        MaskEntry mask[SIZE * HEIGHT];
        int n = 0;

        // Scan the perpendicular plane
        for (x[v] = 0; x[v] < chunkSize[v]; ++x[v]) {
            for (x[u] = 0; x[u] < chunkSize[u]; ++x[u]) {
                // Get blocks on both sides of the current slice
                BlockType blockCurrent = (x[axis] >= 0) ? getBlock(x[0], x[1], x[2]) : BlockType::Air;
                BlockType blockNext = (x[axis] < chunkSize[axis] - 1) ?
                    getBlock(x[0] + q[0], x[1] + q[1], x[2] + q[2]) : BlockType::Air;

                // Check neighbor chunks if at boundary
                if (x[axis] == chunkSize[axis] - 1 && world) {
                    int worldX = chunkPosition.x * SIZE + x[0] + q[0];
                    int worldY = x[1] + q[1];
                    int worldZ = chunkPosition.z * SIZE + x[2] + q[2];
                    blockNext = world->getBlock(worldX, worldY, worldZ);
                }

                // Check if we need to render a face here
                bool currentOpaque = isBlockOpaque(blockCurrent);
                bool nextOpaque = isBlockOpaque(blockNext);

                if (currentOpaque == nextOpaque) {
                    // No face needed (both solid or both air)
                    mask[n++] = {BlockType::Air, BlockFace::Top};
                } else if (currentOpaque) {
                    // Render positive face (current block facing +axis direction)
                    mask[n++] = {blockCurrent, positiveFace};
                } else {
                    // Render negative face (next block facing -axis direction)
                    mask[n++] = {blockNext, negativeFace};
                }
            }
        }

        ++x[axis];
        n = 0;

        // Generate mesh from mask using greedy meshing
        for (int j = 0; j < chunkSize[v]; ++j) {
            for (int i = 0; i < chunkSize[u]; ) {
                if (mask[n].blockType != BlockType::Air) {
                    BlockType currentBlock = mask[n].blockType;
                    BlockFace currentFace = mask[n].face;

                    // Compute width - merge quads with same block type AND face
                    int w;
                    for (w = 1; i + w < chunkSize[u] &&
                         mask[n + w].blockType == currentBlock &&
                         mask[n + w].face == currentFace; ++w) {}

                    // Compute height
                    bool done = false;
                    int h;
                    for (h = 1; j + h < chunkSize[v]; ++h) {
                        for (int k = 0; k < w; ++k) {
                            if (mask[n + k + h * chunkSize[u]].blockType != currentBlock ||
                                mask[n + k + h * chunkSize[u]].face != currentFace) {
                                done = true;
                                break;
                            }
                        }
                        if (done) break;
                    }

                    // Add quad with dimensions w x h
                    x[u] = i;
                    x[v] = j;

                    int du[3] = {0, 0, 0};
                    int dv[3] = {0, 0, 0};
                    du[u] = w;
                    dv[v] = h;

                    addGreedyQuad(meshData, x, du, dv, currentBlock, currentFace, atlas, w, h);

                    // Clear the mask for the quad we just added
                    for (int l = 0; l < h; ++l) {
                        for (int k = 0; k < w; ++k) {
                            mask[n + k + l * chunkSize[u]].blockType = BlockType::Air;
                        }
                    }

                    i += w;
                    n += w;
                } else {
                    ++i;
                    ++n;
                }
            }
        }
    }
}

void Chunk::addGreedyQuad(MeshData& meshData, int x[3], int du[3], int dv[3],
                          BlockType block, BlockFace face, const TextureAtlas *atlas, int width, int height) {
    // Calculate the four corners of the quad
    glm::u8vec3 v1(x[0], x[1], x[2]);
    glm::u8vec3 v2(x[0] + du[0], x[1] + du[1], x[2] + du[2]);
    glm::u8vec3 v3(x[0] + du[0] + dv[0], x[1] + du[1] + dv[1], x[2] + du[2] + dv[2]);
    glm::u8vec3 v4(x[0] + dv[0], x[1] + dv[1], x[2] + dv[2]);

    // Get the tile index for this block face
    uint8_t tileIndex = atlas->getBlockFaceTileIndex(block, face);

    // Get normal ID for this face
    uint8_t normalId = static_cast<uint8_t>(face);

    uint32_t baseIndex = meshData.vertices.size();

    uint8_t c0, c1, c2, c3;
    uint8_t w, h;
    if (face == BlockFace::Front || face == BlockFace::Back) {
        // Need to rotate UVs 90 degrees clockwise for Z-axis
        c0 = 1;
        c1 = 2;
        c2 = 3;
        c3 = 0;
        // AND swap width/height so tiling works correctly
        w = static_cast<uint8_t>(height);
        h = static_cast<uint8_t>(width);
    } else {
        // Normal orientation for X and Y faces
        c0 = 0;
        c1 = 1;
        c2 = 2;
        c3 = 3;
        w = static_cast<uint8_t>(width);
        h = static_cast<uint8_t>(height);
    }

    meshData.vertices.push_back({v1, tileIndex, c0, normalId, w, h});
    meshData.vertices.push_back({v2, tileIndex, c1, normalId, w, h});
    meshData.vertices.push_back({v3, tileIndex, c2, normalId, w, h});
    meshData.vertices.push_back({v4, tileIndex, c3, normalId, w, h});

    // Add indices (two triangles)
    bool backFace = (static_cast<int>(face) % 2 == 1);

    if (backFace) {
        meshData.indices.push_back(baseIndex);
        meshData.indices.push_back(baseIndex + 2);
        meshData.indices.push_back(baseIndex + 1);
        meshData.indices.push_back(baseIndex);
        meshData.indices.push_back(baseIndex + 3);
        meshData.indices.push_back(baseIndex + 2);
    } else {
        meshData.indices.push_back(baseIndex);
        meshData.indices.push_back(baseIndex + 1);
        meshData.indices.push_back(baseIndex + 2);
        meshData.indices.push_back(baseIndex);
        meshData.indices.push_back(baseIndex + 2);
        meshData.indices.push_back(baseIndex + 3);
    }
}

void Chunk::uploadMeshToGPU(const MeshData& meshData) {
    PROFILE_SCOPE("Chunk::uploadMeshToGPU");
    chunkMesh.setData(meshData.vertices, meshData.indices);
    chunkDirty = false;
    chunkState.store(ChunkState::Ready);
}

bool Chunk::isBlockAt(int x, int y, int z) const {
    if (x < 0 || x >= SIZE || y < 0 || y >= HEIGHT || z < 0 || z >= SIZE) {
        return false; // Treat out-of-bounds as air
    }
    return isBlockOpaque(chunkBlocks[x][y][z]);
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
        return !isBlockOpaque(chunkBlocks[neighborX][neighborY][neighborZ]);
    }

    // TODO: Currently doesn't work if the chunk is still generating
    if (world) {
        int worldX = chunkPosition.x * SIZE + neighborX;
        int worldY = neighborY;
        int worldZ = chunkPosition.z * SIZE + neighborZ;
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
    if (!chunkMesh.isEmpty()) {
        chunkMesh.draw();
    }
}
