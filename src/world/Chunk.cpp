//
// Created by maxim on 03/01/2026.
//

#include "Chunk.h"
#include "src/debug/TestScene.h"
#include <algorithm>
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
    : chunkPosition(position), chunkState(ChunkState::Empty) {
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
    if (type == BlockType::Water) {
        hasWater = true;
        waterMinY = std::min(waterMinY, y);
        waterMaxY = std::max(waterMaxY, y);
    } else if (type != BlockType::Air) {
        solidMinY = std::min(solidMinY, y);
        solidMaxY = std::max(solidMaxY, y);
    }
    editVersion.fetch_add(1);
}

namespace {

bool blockInPass(BlockType type, MeshPass pass) {
    if (pass == MeshPass::Water) {
        return type == BlockType::Water;
    }
    return type != BlockType::Air && type != BlockType::Water;
}

uint32_t columnSeed(int worldX, int worldZ, uint32_t worldSeed) {
    uint32_t h = worldSeed ^ (static_cast<uint32_t>(worldX) * 73856093u) ^ (static_cast<uint32_t>(worldZ) * 19349663u);
    h = (h ^ 61u) ^ (h >> 16);
    h += (h << 3);
    h ^= (h >> 4);
    h *= 0x27d4eb2du;
    h ^= (h >> 15);
    return h;
}

}

ChunkBlockSink::ChunkBlockSink(Chunk& _chunk)
    : chunk(_chunk),
      origin(_chunk.getPosition().x * Chunk::SIZE, 0, _chunk.getPosition().z * Chunk::SIZE) {}

void ChunkBlockSink::set(int worldX, int worldY, int worldZ, BlockType type) {
    // Chunk::setBlock bounds-checks, so out-of-chunk writes drop here by design.
    chunk.setBlock(worldX - origin.x, worldY - origin.y, worldZ - origin.z, type);
}

void Chunk::generate(const siv::PerlinNoise* perlinNoise, WorleyBiome* worleyBiome) {
    // Fixed geometry
    if (TestScene::enabled) {
        generateTestSceneTerrain();
        chunkState.store(ChunkState::Generated);
        return;
    }

    generateTerrain(perlinNoise, worleyBiome);
    decorateTerrain(perlinNoise, worleyBiome);
    // After decoration, so trunks overwrites grass e.g.
    placeStructures(perlinNoise, worleyBiome);

    chunkState.store(ChunkState::Generated);
}

void Chunk::generateTerrain(const siv::PerlinNoise* perlinNoise, WorleyBiome* worleyBiome) {
    for (int x = 0; x < SIZE; x++) {
        for (int z = 0; z < SIZE; z++) {
            int worldX = chunkPosition.x * SIZE + x;
            int worldZ = chunkPosition.z * SIZE + z;
            const Biome* biome = worleyBiome->getBiomeAt(worldX, worldZ);

            float height = worleyBiome->getBlendedHeight(worldX, worldZ, perlinNoise);
            int terrainHeight = static_cast<int>(height);

            int surfaceDepth = biome->getSurfaceDepth();
            int subSurfaceDepth = biome->getSubSurfaceDepth();

            // anything near the waterline turns to sand.
            const bool shore = terrainHeight <= SEA_LEVEL + BEACH_HEIGHT;

            // Per column rather than per block: the bands follow straight from the height.
            if (terrainHeight > 0) {
                solidMinY = 0;
                solidMaxY = std::max(solidMaxY, std::min(terrainHeight, HEIGHT) - 1);
            }
            if (terrainHeight < SEA_LEVEL) {
                waterMinY = std::min(waterMinY, std::max(terrainHeight, 0));
                waterMaxY = std::max(waterMaxY, SEA_LEVEL - 1);
            }

            for (int y = 0; y < HEIGHT; y++) {
                if (y < terrainHeight - surfaceDepth - subSurfaceDepth) {
                    chunkBlocks[x][y][z] = biome->getStoneBlock(worldX, y, worldZ);
                }
                else if (y < terrainHeight - surfaceDepth) {
                    chunkBlocks[x][y][z] = shore ? BlockType::Sand
                                                 : biome->getSubSurfaceBlock(worldX, y, worldZ);
                }
                else if (y < terrainHeight) {
                    chunkBlocks[x][y][z] = shore ? BlockType::Sand
                                                 : biome->getSurfaceBlock(worldX, y, worldZ);
                }
                else if (y < SEA_LEVEL) {
                    chunkBlocks[x][y][z] = BlockType::Water;
                    hasWater = true;
                }
                else {
                    chunkBlocks[x][y][z] = BlockType::Air;
                }
            }
        }
    }
}

// TODO: Make it more generic for future scenes ?
void Chunk::generateTestSceneTerrain() {
    using namespace TestScene;

    const int originX = chunkPosition.x * SIZE;
    const int originZ = chunkPosition.z * SIZE;

    // Uniform stone, so banding is not competing with texture detail
    for (int x = 0; x < SIZE; x++)
        for (int z = 0; z < SIZE; z++)
            for (int y = 0; y < HEIGHT; y++)
                chunkBlocks[x][y][z] = y < GROUND_Y ? BlockType::Stone : BlockType::Air;

    solidMinY = 0;
    solidMaxY = GROUND_Y - 1;

    // Props are addressed in world space and clipped to this chunk, so one straddling a
    // chunk seam still comes out whole.
    auto put = [&](int wx, int wy, int wz) {
        const int lx = wx - originX;
        const int lz = wz - originZ;
        if (lx < 0 || lx >= SIZE || lz < 0 || lz >= SIZE) return;
        if (wy < 0 || wy >= HEIGHT) return;
        chunkBlocks[lx][wy][lz] = BlockType::Stone;
        solidMaxY = std::max(solidMaxY, wy);
    };

    put(LONE_BLOCK_X, GROUND_Y, LONE_BLOCK_Z);

    for (int z = WALL_Z0; z <= WALL_Z1; z++)
        for (int y = GROUND_Y; y < WALL_TOP; y++)
            put(WALL_X, y, z);

    for (int x = CUBE_X0; x <= CUBE_X1; x++)
        for (int z = CUBE_Z0; z <= CUBE_Z1; z++)
            for (int y = GROUND_Y; y < CUBE_TOP; y++)
                put(x, y, z);

    for (int i = 0; i <= STAIR_Z1 - STAIR_Z0; i++)
        for (int x = STAIR_X0; x < STAIR_X0 + 5; x++)
            for (int y = GROUND_Y; y <= GROUND_Y + i; y++)
                put(x, y, STAIR_Z0 + i);

    for (int x = GRAZE_WALL_X0; x <= GRAZE_WALL_X1; x++)
        for (int y = GROUND_Y; y < GRAZE_WALL_TOP; y++)
            put(x, y, GRAZE_WALL_Z);

    for (int y = GROUND_Y; y < SLAB_Y; y++) {
        put(SLAB_X0, y, SLAB_Z0);
        put(SLAB_X0, y, SLAB_Z1);
    }
    for (int x = SLAB_X0; x <= SLAB_X1; x++)
        for (int z = SLAB_Z0; z <= SLAB_Z1; z++)
            put(x, SLAB_Y, z);
}

void Chunk::decorateTerrain(const siv::PerlinNoise* perlinNoise, WorleyBiome* worleyBiome) {
    for (int x = 0; x < SIZE; x++) {
        for (int z = 0; z < SIZE; z++) {
            // Water is not a ground block, so this lands on the seabed in flooded columns.
            int surfaceY = getTerrainHeight(x, z);

            if (surfaceY >= SEA_LEVEL && surfaceY < HEIGHT) {
                int worldX = chunkPosition.x * SIZE + x;
                int worldZ = chunkPosition.z * SIZE + z;

                uint32_t seed = columnSeed(worldX, worldZ, worleyBiome->getSeed());

                const Biome* biome = worleyBiome->getBiomeAt(worldX, worldZ);
                biome->decorate(this, x, z, surfaceY, perlinNoise, seed);
            }
        }
    }
}

void Chunk::placeStructures(const siv::PerlinNoise* perlinNoise, WorleyBiome* worleyBiome) {
    ChunkBlockSink sink(*this);
    const uint32_t worldSeed = worleyBiome->getSeed();
    const TerrainSampler terrain{worleyBiome, perlinNoise};

    // sweeps past the chunk to verify if it needs to place a structure
    for (int x = -STRUCTURE_MARGIN; x < SIZE + STRUCTURE_MARGIN; x++) {
        for (int z = -STRUCTURE_MARGIN; z < SIZE + STRUCTURE_MARGIN; z++) {
            const int worldX = chunkPosition.x * SIZE + x;
            const int worldZ = chunkPosition.z * SIZE + z;

            const Biome* biome = worleyBiome->getBiomeAt(worldX, worldZ);
            if (!biome->canSpawnStructures()) {
                continue;
            }

            // Height comes from noise
            const int surfaceY = static_cast<int>(
                worleyBiome->getBlendedHeight(worldX, worldZ, perlinNoise));
            if (surfaceY < SEA_LEVEL || surfaceY >= HEIGHT) {
                continue;
            }

            biome->placeStructure(sink, worldX, surfaceY, worldZ,
                                  columnSeed(worldX, worldZ, worldSeed), terrain);
        }
    }
}

int Chunk::getTerrainHeight(int localX, int localZ) const {
    for (int y = HEIGHT - 1; y >= 0; y--) {
        if (isGroundBlock(getBlock(localX, y, localZ))) {
            return y + 1;
        }
    }
    return 0;
}

void Chunk::buildMeshData(MeshData& meshData, const TextureAtlas *atlas,
                          const ChunkNeighbourhood& neighbours) {
    PROFILE_SCOPE("Chunk::buildMeshData");

    meshData.clear();
    meshData.vertices.reserve(SIZE * SIZE * 4);
    meshData.indices.reserve(SIZE * SIZE * 6);

    greedyMeshAxis(meshData, atlas, neighbours, 0, MeshPass::Opaque);
    greedyMeshAxis(meshData, atlas, neighbours, 1, MeshPass::Opaque);
    greedyMeshAxis(meshData, atlas, neighbours, 2, MeshPass::Opaque);

    chunkState.store(ChunkState::MeshBuilt);
}

void Chunk::buildWaterMeshData(MeshData& meshData, const TextureAtlas *atlas,
                               const ChunkNeighbourhood& neighbours) {
    PROFILE_SCOPE("Chunk::buildWaterMeshData");

    meshData.clear();
    if (!hasWater) return;

    meshData.vertices.reserve(SIZE * SIZE);
    meshData.indices.reserve(SIZE * SIZE * 2);

    greedyMeshAxis(meshData, atlas, neighbours, 0, MeshPass::Water);
    greedyMeshAxis(meshData, atlas, neighbours, 1, MeshPass::Water);
    greedyMeshAxis(meshData, atlas, neighbours, 2, MeshPass::Water);
}

void Chunk::buildTransparentMeshData(MeshData& meshData, const TextureAtlas *atlas) {
    PROFILE_SCOPE("Chunk::buildTransparentMeshData");

    meshData.clear();
    meshData.vertices.reserve(SIZE * SIZE);
    meshData.indices.reserve(SIZE * SIZE * 2);

    for (int x = 0; x < SIZE; x++) {
        for (int y = 0; y < HEIGHT; y++) {
            for (int z = 0; z < SIZE; z++) {
                BlockType blockType = getBlock(x, y, z);

                if (getBlockRenderType(blockType) == BlockRenderType::CrossModel) {
                    addCrossModel(meshData, glm::vec3(x, y, z), blockType, atlas);
                }
            }
        }
    }
}

void Chunk::greedyMeshAxis(MeshData& meshData, const TextureAtlas *atlas,
                           const ChunkNeighbourhood& neighbours, int axis, MeshPass pass) {
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

    // Full extent on X and Z, but only the occupied band on Y. Terrain tops out around a
    // quarter of the height and water is thinner still, so sweeping all 256 levels spends
    // most of its time on guaranteed-empty space.
    int lo[3] = {0, 0, 0};
    int hi[3] = {SIZE - 1, HEIGHT - 1, SIZE - 1};
    if (pass == MeshPass::Water) {
        if (waterMaxY < waterMinY) return;
        lo[1] = waterMinY;
        hi[1] = waterMaxY;
    } else {
        if (solidMaxY < solidMinY) return;
        lo[1] = solidMinY;
        hi[1] = solidMaxY;
    }

    // For each slice along the axis
    for (x[axis] = lo[axis] - 1; x[axis] <= hi[axis]; ) {
        // Build mask for this slice
        struct MaskEntry {
            BlockType blockType;
            BlockFace face;
        };

        // Indexed explicitly by (v, u) with the full stride rather than by a running
        // counter, so the clamped band addresses the same slots it always did. Entries
        // outside the band are never written and never read.
        MaskEntry mask[SIZE * HEIGHT];

        // Scan the perpendicular plane
        for (x[v] = lo[v]; x[v] <= hi[v]; ++x[v]) {
            for (x[u] = lo[u]; x[u] <= hi[u]; ++x[u]) {
                // Get blocks on both sides of the current slice
                BlockType blockCurrent = (x[axis] >= 0) ? getBlock(x[0], x[1], x[2]) : BlockType::Air;
                BlockType blockNext = (x[axis] < chunkSize[axis] - 1) ?
                    getBlock(x[0] + q[0], x[1] + q[1], x[2] + q[2]) : BlockType::Air;

                // Check neighbor chunks if at boundary
                // Check at the far boundary
                if (x[axis] == chunkSize[axis] - 1) {
                    int worldX = chunkPosition.x * SIZE + x[0] + q[0];
                    int worldY = x[1] + q[1];
                    int worldZ = chunkPosition.z * SIZE + x[2] + q[2];
                    blockNext = neighbours.getBlock(worldX, worldY, worldZ);
                }
                // Also check at the near boundary (when x[axis] == -1)
                else if (x[axis] == -1) {
                    int worldX = chunkPosition.x * SIZE + x[0];
                    int worldY = x[1];
                    int worldZ = chunkPosition.z * SIZE + x[2];
                    blockCurrent = neighbours.getBlock(worldX, worldY, worldZ);
                }

                if (getBlockRenderType(blockCurrent) == BlockRenderType::CrossModel) {
                    blockCurrent = BlockType::Air;
                }
                if (getBlockRenderType(blockNext) == BlockRenderType::CrossModel) {
                    blockNext = BlockType::Air;
                }

                // A chunk emits only faces of blocks it owns. The block across a seam belongs
                // to the neighbour, which emits that face from its own side -- without this
                // every seam face was built twice, once into each chunk's mesh.
                const bool currentIsOurs = (x[axis] >= 0);
                const bool nextIsOurs = (x[axis] < chunkSize[axis] - 1);

                // Water/water and solid/solid both fall out of the != test, so no interior
                // faces reach the blend and the sea cannot stack into opacity.
                bool drawCurrent = currentIsOurs && blockInPass(blockCurrent, pass) && blockCurrent != blockNext &&!isBlockOpaque(blockNext);
                bool drawNext = nextIsOurs && blockInPass(blockNext, pass) && blockNext != blockCurrent &&!isBlockOpaque(blockCurrent);

                MaskEntry& slot = mask[x[v] * chunkSize[u] + x[u]];
                if (drawCurrent) {
                    // Current block facing +axis
                    slot = {blockCurrent, positiveFace};
                } else if (drawNext) {
                    // Next block facing -axis
                    slot = {blockNext, negativeFace};
                } else {
                    slot = {BlockType::Air, BlockFace::Top};
                }
            }
        }

        ++x[axis];

        // Generate mesh from mask using greedy meshing
        for (int j = lo[v]; j <= hi[v]; ++j) {
            for (int i = lo[u]; i <= hi[u]; ) {
                const int n = j * chunkSize[u] + i;
                if (mask[n].blockType != BlockType::Air) {
                    BlockType currentBlock = mask[n].blockType;
                    BlockFace currentFace = mask[n].face;

                    // Compute width - merge quads with same block type AND face
                    int w;
                    for (w = 1; i + w <= hi[u] &&
                         mask[n + w].blockType == currentBlock &&
                         mask[n + w].face == currentFace; ++w) {}

                    // Compute height
                    bool done = false;
                    int h;
                    for (h = 1; j + h <= hi[v]; ++h) {
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
                } else {
                    ++i;
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

void Chunk::addCrossModel(MeshData& meshData, const glm::vec3& pos, BlockType blockType,
                         const TextureAtlas* atlas) {
    uint8_t tileIndex = atlas->getBlockFaceTileIndex(blockType, BlockFace::Front);
    uint32_t baseIndex = meshData.vertices.size();

    glm::u8vec3 v1a(static_cast<uint8_t>(pos.x), static_cast<uint8_t>(pos.y), static_cast<uint8_t>(pos.z));
    glm::u8vec3 v2a(static_cast<uint8_t>(pos.x + 1), static_cast<uint8_t>(pos.y), static_cast<uint8_t>(pos.z + 1));
    glm::u8vec3 v3a(static_cast<uint8_t>(pos.x + 1), static_cast<uint8_t>(pos.y + 1), static_cast<uint8_t>(pos.z + 1));
    glm::u8vec3 v4a(static_cast<uint8_t>(pos.x), static_cast<uint8_t>(pos.y + 1), static_cast<uint8_t>(pos.z));

    meshData.vertices.push_back({v1a, tileIndex, 0, 0, 1, 1});
    meshData.vertices.push_back({v2a, tileIndex, 1, 0, 1, 1});
    meshData.vertices.push_back({v3a, tileIndex, 2, 0, 1, 1});
    meshData.vertices.push_back({v4a, tileIndex, 3, 0, 1, 1});

    meshData.indices.push_back(baseIndex + 0);
    meshData.indices.push_back(baseIndex + 1);
    meshData.indices.push_back(baseIndex + 2);
    meshData.indices.push_back(baseIndex + 0);
    meshData.indices.push_back(baseIndex + 2);
    meshData.indices.push_back(baseIndex + 3);

    baseIndex = meshData.vertices.size();

    glm::u8vec3 v1b(static_cast<uint8_t>(pos.x + 1), static_cast<uint8_t>(pos.y), static_cast<uint8_t>(pos.z));
    glm::u8vec3 v2b(static_cast<uint8_t>(pos.x), static_cast<uint8_t>(pos.y), static_cast<uint8_t>(pos.z + 1));
    glm::u8vec3 v3b(static_cast<uint8_t>(pos.x), static_cast<uint8_t>(pos.y + 1), static_cast<uint8_t>(pos.z + 1));
    glm::u8vec3 v4b(static_cast<uint8_t>(pos.x + 1), static_cast<uint8_t>(pos.y + 1), static_cast<uint8_t>(pos.z));

    meshData.vertices.push_back({v1b, tileIndex, 0, 0, 1, 1});
    meshData.vertices.push_back({v2b, tileIndex, 1, 0, 1, 1});
    meshData.vertices.push_back({v3b, tileIndex, 2, 0, 1, 1});
    meshData.vertices.push_back({v4b, tileIndex, 3, 0, 1, 1});

    meshData.indices.push_back(baseIndex + 0);
    meshData.indices.push_back(baseIndex + 1);
    meshData.indices.push_back(baseIndex + 2);
    meshData.indices.push_back(baseIndex + 0);
    meshData.indices.push_back(baseIndex + 2);
    meshData.indices.push_back(baseIndex + 3);
}

void Chunk::uploadMeshToGPU(const MeshData& meshData) {
    PROFILE_SCOPE("Chunk::uploadMeshToGPU");
    chunkMesh.setData(meshData.vertices, meshData.indices);
    chunkState.store(ChunkState::Ready);
}

void Chunk::uploadTransparentMeshToGPU(const MeshData& meshData) {
    PROFILE_SCOPE("Chunk::uploadTransparentMeshToGPU");
    chunkTransparentMesh.setData(meshData.vertices, meshData.indices);
}

void Chunk::uploadWaterMeshToGPU(const MeshData& meshData) {
    PROFILE_SCOPE("Chunk::uploadWaterMeshToGPU");
    chunkWaterMesh.setData(meshData.vertices, meshData.indices);
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
    if (chunkMesh.isEmpty()) return;
    chunkMesh.draw();
}

void Chunk::drawTransparent() const {
    if (chunkTransparentMesh.isEmpty()) return;
    chunkTransparentMesh.draw();
}

void Chunk::drawWater() const {
    if (chunkWaterMesh.isEmpty()) return;
    chunkWaterMesh.draw();
}
