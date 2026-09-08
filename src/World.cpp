#include "World.h"
#include "raymath.h"
#include "rlgl.h"
#include <cmath>
#include <cstring>
#include <algorithm>

World::World(unsigned int seed) : m_seed(seed), m_noise(seed) {}

World::~World() {
    for (auto& [coord, chunk] : m_chunks) {
        if (chunk) chunk->Unload();
    }
    for (auto& [coord, lod] : m_lodChunks) {
        if (lod) lod->Unload();
    }
}

Chunk* World::GetChunk(int cx, int cz) {
    ChunkCoord key{ cx, cz };
    auto it = m_chunks.find(key);
    if (it == m_chunks.end()) return nullptr;
    return it->second.get();
}

const Chunk* World::GetChunk(int cx, int cz) const {
    ChunkCoord key{ cx, cz };
    auto it = m_chunks.find(key);
    if (it == m_chunks.end()) return nullptr;
    return it->second.get();
}

void World::EnsureChunk(int cx, int cz) {
    ChunkCoord key{ cx, cz };
    if (m_chunks.count(key)) return;

    auto chunk = std::make_unique<Chunk>();
    chunk->chunkX = cx;
    chunk->chunkZ = cz;
    GenerateChunk(*chunk);
    m_chunks[key] = std::move(chunk);
}

void World::PlaceTree(Chunk& chunk, int lx, int surfaceY, int lz) {
    int trunkHeight = 4 + (int)(m_noise.Value2D(
        (float)(lx + chunk.chunkX * 16) * 0.7f,
        (float)(lz + chunk.chunkZ * 16) * 0.7f) * 3.0f);

    if (surfaceY + trunkHeight + 2 >= CHUNK_SIZE_Y) return;

    for (int y = 1; y <= trunkHeight; ++y) {
        int ty = surfaceY + y;
        if (chunk.InBounds(lx, ty, lz)) {
            chunk.Set(lx, ty, lz, BlockType::Wood);
        }
    }

    int top = surfaceY + trunkHeight;

    auto setLeaf = [&](int x, int y, int z) {
        if (chunk.InBounds(x, y, z) && chunk.Get(x, y, z).type == BlockType::Air) {
            chunk.Set(x, y, z, BlockType::Leaves);
        }
    };

    auto leafLayer = [&](int centerY, int radius, bool cutCorners) {
        for (int dx = -radius; dx <= radius; ++dx) {
            for (int dz = -radius; dz <= radius; ++dz) {
                if (cutCorners && std::abs(dx) == radius && std::abs(dz) == radius)
                    continue;
                if (dx == 0 && dz == 0 && centerY <= top) continue;
                setLeaf(lx + dx, centerY, lz + dz);
            }
        }
    };

    leafLayer(top - 1, 2, true);
    leafLayer(top,     2, true);
    leafLayer(top + 1, 1, false);
    leafLayer(top + 2, 1, true);
}

void World::GenerateChunk(Chunk& chunk) {
    const int baseX = chunk.chunkX * CHUNK_SIZE_X;
    const int baseZ = chunk.chunkZ * CHUNK_SIZE_Z;

    constexpr int SEA_LEVEL = 28;
    constexpr int BEACH_HEIGHT = 2;

    for (int x = 0; x < CHUNK_SIZE_X; ++x) {
        for (int z = 0; z < CHUNK_SIZE_Z; ++z) {
            float wx = static_cast<float>(baseX + x);
            float wz = static_cast<float>(baseZ + z);

            float n = m_noise.FBM2D(wx * 0.02f, wz * 0.02f, 5, 2.0f, 0.5f);
            int height = static_cast<int>(22 + n * 26.0f);
            height = std::clamp(height, 8, CHUNK_SIZE_Y - 8);

            for (int y = 0; y < CHUNK_SIZE_Y; ++y) {
                BlockType type = BlockType::Air;

                if (y == 0) {
                    type = BlockType::Bedrock;
                }
                else if (y < height - 4) {
                    type = BlockType::Stone;
                }
                else if (y < height) {
                    if (height < SEA_LEVEL + BEACH_HEIGHT) {
                        type = BlockType::Sand;
                    } else {
                        type = BlockType::Dirt;
                    }
                }
                else if (y == height) {
                    if (height <= SEA_LEVEL + BEACH_HEIGHT) {
                        type = BlockType::Sand;
                    } else {
                        type = BlockType::Grass;
                    }
                }

                if (type == BlockType::Air && y <= SEA_LEVEL) {
                    type = BlockType::Water;
                }

                chunk.blocks[x][y][z].type = type;
            }

            if (height > SEA_LEVEL + 2) {
                float treeNoise = m_noise.Value2D(wx * 0.12f + 40.0f, wz * 0.12f + 40.0f);
                if (treeNoise > 0.82f && height < CHUNK_SIZE_Y - 12) {
                    if (x > 2 && x < CHUNK_SIZE_X - 3 && z > 2 && z < CHUNK_SIZE_Z - 3) {
                        PlaceTree(chunk, x, height, z);
                    }
                }
            }
        }
    }

    chunk.generated = true;
    chunk.dirty = true;
}

void World::EnsureLodChunk(int cx, int cz, int lodLevel) {
    ChunkCoord key{ cx, cz + lodLevel * 100000 };

    if (m_lodChunks.count(key)) return;

    auto lod = std::make_unique<LodChunk>();
    lod->chunkX = cx;
    lod->chunkZ = cz;
    lod->lodLevel = lodLevel;
    GenerateLodMesh(*lod);
    m_lodChunks[key] = std::move(lod);
}

namespace {
    struct LodMeshBuffers {
        std::vector<Vector3> vertices;
        std::vector<Color>   colors;
        std::vector<unsigned short> indices;
    };

    void AddLodQuad(LodMeshBuffers& buf, Vector3 v0, Vector3 v1, Vector3 v2, Vector3 v3, Color col) {
        unsigned short base = (unsigned short)buf.vertices.size();
        buf.vertices.push_back(v0);
        buf.vertices.push_back(v1);
        buf.vertices.push_back(v2);
        buf.vertices.push_back(v3);
        for (int i = 0; i < 4; ++i) buf.colors.push_back(col);

        buf.indices.push_back(base + 0);
        buf.indices.push_back(base + 1);
        buf.indices.push_back(base + 2);
        buf.indices.push_back(base + 0);
        buf.indices.push_back(base + 2);
        buf.indices.push_back(base + 3);

        buf.indices.push_back(base + 2);
        buf.indices.push_back(base + 1);
        buf.indices.push_back(base + 0);
        buf.indices.push_back(base + 3);
        buf.indices.push_back(base + 2);
        buf.indices.push_back(base + 0);
    }

    bool UploadLodBuffers(LodMeshBuffers& buf, Mesh& outMesh, Model& outModel) {
        if (buf.vertices.empty()) return false;

        outMesh = { 0 };
        outMesh.vertexCount   = (int)buf.vertices.size();
        outMesh.triangleCount = (int)(buf.indices.size() / 3);

        outMesh.vertices = (float*)MemAlloc(outMesh.vertexCount * 3 * sizeof(float));
        outMesh.colors   = (unsigned char*)MemAlloc(outMesh.vertexCount * 4 * sizeof(unsigned char));
        outMesh.indices  = (unsigned short*)MemAlloc(buf.indices.size() * sizeof(unsigned short));

        for (int i = 0; i < outMesh.vertexCount; ++i) {
            outMesh.vertices[i*3+0] = buf.vertices[i].x;
            outMesh.vertices[i*3+1] = buf.vertices[i].y;
            outMesh.vertices[i*3+2] = buf.vertices[i].z;
            outMesh.colors[i*4+0]   = buf.colors[i].r;
            outMesh.colors[i*4+1]   = buf.colors[i].g;
            outMesh.colors[i*4+2]   = buf.colors[i].b;
            outMesh.colors[i*4+3]   = buf.colors[i].a;
        }
        memcpy(outMesh.indices, buf.indices.data(), buf.indices.size() * sizeof(unsigned short));

        UploadMesh(&outMesh, false);
        outModel = LoadModelFromMesh(outMesh);
        return true;
    }
}

void World::GenerateLodMesh(LodChunk& lod) {
    const int scale = (lod.lodLevel == 1) ? 2 : 4;
    const int size  = 16 / scale;

    LodMeshBuffers opaque;
    LodMeshBuffers water;

    const int baseX = lod.chunkX * 16;
    const int baseZ = lod.chunkZ * 16;
    constexpr int SEA_LEVEL = 28;

    std::vector<int> heights(size * size);

    for (int lx = 0; lx < size; ++lx) {
        for (int lz = 0; lz < size; ++lz) {
            float wx = (float)(baseX + lx * scale + scale / 2);
            float wz = (float)(baseZ + lz * scale + scale / 2);

            float n = m_noise.FBM2D(wx * 0.02f, wz * 0.02f, 4, 2.0f, 0.5f);
            int height = (int)(22 + n * 26.0f);
            height = std::clamp(height, 8, 60);
            heights[lz * size + lx] = height;

            BlockType surfaceType = (height <= SEA_LEVEL + 2) ? BlockType::Sand : BlockType::Grass;
            Color col = GetBlockColor(surfaceType, 0);
            Color sideCol = {
                (unsigned char)(col.r * 0.8f),
                (unsigned char)(col.g * 0.8f),
                (unsigned char)(col.b * 0.8f),
                255
            };

            float x0 = (float)(baseX + lx * scale);
            float z0 = (float)(baseZ + lz * scale);
            float y0 = (float)(height - scale);
            float y1 = (float)height;
            float s  = (float)scale;

            AddLodQuad(opaque, { x0, y1, z0 },
                               { x0, y1, z0 + s },
                               { x0 + s, y1, z0 + s },
                               { x0 + s, y1, z0 }, col);

            AddLodQuad(opaque, { x0,     y0, z0 + s }, { x0 + s, y0, z0 + s }, { x0 + s, y1, z0 + s }, { x0, y1, z0 + s }, sideCol);
            AddLodQuad(opaque, { x0 + s, y0, z0     }, { x0,     y0, z0     }, { x0,     y1, z0     }, { x0 + s, y1, z0 }, sideCol);
            AddLodQuad(opaque, { x0 + s, y0, z0     }, { x0 + s, y0, z0 + s }, { x0 + s, y1, z0 + s }, { x0 + s, y1, z0 }, sideCol);
            AddLodQuad(opaque, { x0,     y0, z0 + s }, { x0,     y0, z0     }, { x0,     y1, z0     }, { x0,     y1, z0 + s }, sideCol);

            if (height < SEA_LEVEL) {
                Color waterCol = GetBlockColor(BlockType::Water, 0);
                float waterY = (float)SEA_LEVEL - 0.1f;

                AddLodQuad(water, { x0,     waterY, z0     },
                                  { x0,     waterY, z0 + s },
                                  { x0 + s, waterY, z0 + s },
                                  { x0 + s, waterY, z0     }, waterCol);
            }
        }
    }

    if (lod.lodLevel == 1) {
        for (int lx = 1; lx < size - 1; ++lx) {
            for (int lz = 1; lz < size - 1; ++lz) {
                int height = heights[lz * size + lx];
                if (height <= SEA_LEVEL + 3) continue;

                float wx = (float)(baseX + lx * scale + scale / 2);
                float wz = (float)(baseZ + lz * scale + scale / 2);

                float treeNoise = m_noise.Value2D(wx * 0.12f + 40.0f, wz * 0.12f + 40.0f);
                if (treeNoise < 0.84f) continue;

                float tx = (float)(baseX + lx * scale + scale / 2 - 0.5f);
                float tz = (float)(baseZ + lz * scale + scale / 2 - 0.5f);
                float ty = (float)height;

                Color trunkCol = GetBlockColor(BlockType::Wood, 1);
                Color leafCol  = GetBlockColor(BlockType::Leaves, 0);

                auto addBox = [&](float x0, float y0, float z0,
                                  float x1, float y1, float z1,
                                  Color boxCol) {
                    AddLodQuad(opaque, { x0, y0, z0 },
                                       { x1, y0, z0 },
                                       { x1, y0, z1 },
                                       { x0, y0, z1 }, boxCol);

                    AddLodQuad(opaque, { x0, y1, z1 },
                                       { x1, y1, z1 },
                                       { x1, y1, z0 },
                                       { x0, y1, z0 }, boxCol);

                    AddLodQuad(opaque, { x0, y0, z1 },
                                       { x1, y0, z1 },
                                       { x1, y1, z1 },
                                       { x0, y1, z1 }, boxCol);

                    AddLodQuad(opaque, { x1, y0, z0 },
                                       { x0, y0, z0 },
                                       { x0, y1, z0 },
                                       { x1, y1, z0 }, boxCol);

                    AddLodQuad(opaque, { x1, y0, z1 },
                                       { x1, y0, z0 },
                                       { x1, y1, z0 },
                                       { x1, y1, z1 }, boxCol);

                    AddLodQuad(opaque, { x0, y0, z0 },
                                       { x0, y0, z1 },
                                       { x0, y1, z1 },
                                       { x0, y1, z0 }, boxCol);
                };

                float tw = 0.6f;
                float trunkBottom = ty;
                float trunkTop = ty + 3.5f;

                addBox(tx, trunkBottom, tz,
                       tx + tw, trunkTop, tz + tw,
                       trunkCol);

                float ly = ty + 3.0f;
                float ls = 2.2f;
                float lx0 = tx + tw / 2.0f - ls / 2.0f;
                float lz0 = tz + tw / 2.0f - ls / 2.0f;

                addBox(lx0, ly, lz0,
                       lx0 + ls, ly + ls, lz0 + ls,
                       leafCol);
            }
        }
    }

    lod.hasMesh = UploadLodBuffers(opaque, lod.mesh, lod.model);
    lod.hasWaterMesh = UploadLodBuffers(water, lod.waterMesh, lod.waterModel);
    lod.generated = true;
}

void World::Update(const Vector3& playerPos) {
    int pcx = (int)std::floor(playerPos.x / 16.0f);
    int pcz = (int)std::floor(playerPos.z / 16.0f);

    for (int dx = -LOD0_DISTANCE; dx <= LOD0_DISTANCE; ++dx) {
        for (int dz = -LOD0_DISTANCE; dz <= LOD0_DISTANCE; ++dz) {
            EnsureChunk(pcx + dx, pcz + dz);
        }
    }

    for (int dx = -LOD1_DISTANCE; dx <= LOD1_DISTANCE; ++dx) {
        for (int dz = -LOD1_DISTANCE; dz <= LOD1_DISTANCE; ++dz) {
            int dist = std::max(std::abs(dx), std::abs(dz));
            if (dist > LOD0_DISTANCE) {
                EnsureLodChunk(pcx + dx, pcz + dz, 1);
            }
        }
    }

    for (int dx = -LOD2_DISTANCE; dx <= LOD2_DISTANCE; ++dx) {
        for (int dz = -LOD2_DISTANCE; dz <= LOD2_DISTANCE; ++dz) {
            int dist = std::max(std::abs(dx), std::abs(dz));
            if (dist > LOD1_DISTANCE) {
                EnsureLodChunk(pcx + dx, pcz + dz, 2);
            }
        }
    }

    std::vector<ChunkCoord> chunksToRemove;
    for (auto& [coord, chunk] : m_chunks) {
        if (std::abs(coord.x - pcx) > LOD0_DISTANCE + 1 ||
            std::abs(coord.z - pcz) > LOD0_DISTANCE + 1) {
            chunksToRemove.push_back(coord);
        }
    }
    for (auto& c : chunksToRemove) {
        if (m_chunks[c]) m_chunks[c]->Unload();
        m_chunks.erase(c);
    }

    std::vector<ChunkCoord> lodsToRemove;
    for (auto& [key, lod] : m_lodChunks) {
        if (!lod) continue;

        int dist = std::max(std::abs(lod->chunkX - pcx), std::abs(lod->chunkZ - pcz));

        bool shouldRemove = false;

        if (dist <= LOD0_DISTANCE) {
            shouldRemove = true;
        }
        else if (lod->lodLevel == 1 && dist <= LOD0_DISTANCE) {
            shouldRemove = true;
        }
        else if (lod->lodLevel == 2 && dist <= LOD1_DISTANCE) {
            shouldRemove = true;
        }
        else if (dist > LOD2_DISTANCE + 2) {
            shouldRemove = true;
        }

        if (shouldRemove) {
            lodsToRemove.push_back(key);
        }
    }

    for (auto& key : lodsToRemove) {
        if (m_lodChunks[key]) {
            m_lodChunks[key]->Unload();
        }
        m_lodChunks.erase(key);
    }

    for (auto& [coord, chunk] : m_chunks) {
        if (chunk && chunk->dirty) {
            chunk->BuildMesh(*this);
        }
    }
}

void World::Draw() const {
    for (const auto& [coord, chunk] : m_chunks) {
        if (chunk && chunk->hasMesh) {
            DrawModel(chunk->model, { 0, 0, 0 }, 1.0f, WHITE);
        }
    }

    for (const auto& [coord, lod] : m_lodChunks) {
        if (lod && lod->hasMesh) {
            DrawModel(lod->model, { 0, 0, 0 }, 1.0f, WHITE);
        }
    }

    BeginBlendMode(BLEND_ALPHA);
    rlDisableDepthMask();

    for (const auto& [coord, chunk] : m_chunks) {
        if (chunk && chunk->hasWaterMesh) {
            DrawModel(chunk->waterModel, { 0, 0, 0 }, 1.0f, WHITE);
        }
    }

    for (const auto& [coord, lod] : m_lodChunks) {
        if (lod && lod->hasWaterMesh) {
            DrawModel(lod->waterModel, { 0, 0, 0 }, 1.0f, WHITE);
        }
    }

    rlEnableDepthMask();
    EndBlendMode();
}

Block World::GetBlock(int wx, int wy, int wz) const {
    if (wy < 0 || wy >= CHUNK_SIZE_Y) return Block{ BlockType::Air };
    int cx = (int)std::floor((float)wx / CHUNK_SIZE_X);
    int cz = (int)std::floor((float)wz / CHUNK_SIZE_Z);
    const Chunk* c = GetChunk(cx, cz);
    if (!c) return Block{ BlockType::Air };
    int lx = wx - cx * CHUNK_SIZE_X;
    int lz = wz - cz * CHUNK_SIZE_Z;
    return c->Get(lx, wy, lz);
}

void World::SetBlock(int wx, int wy, int wz, BlockType type) {
    if (wy < 0 || wy >= CHUNK_SIZE_Y) return;
    int cx = (int)std::floor((float)wx / CHUNK_SIZE_X);
    int cz = (int)std::floor((float)wz / CHUNK_SIZE_Z);
    Chunk* c = GetChunk(cx, cz);
    if (!c) return;
    int lx = wx - cx * CHUNK_SIZE_X;
    int lz = wz - cz * CHUNK_SIZE_Z;
    c->Set(lx, wy, lz, type);

    if (lx == 0)              if (auto* n = GetChunk(cx - 1, cz)) n->dirty = true;
    if (lx == CHUNK_SIZE_X-1) if (auto* n = GetChunk(cx + 1, cz)) n->dirty = true;
    if (lz == 0)              if (auto* n = GetChunk(cx, cz - 1)) n->dirty = true;
    if (lz == CHUNK_SIZE_Z-1) if (auto* n = GetChunk(cx, cz + 1)) n->dirty = true;
}

bool World::Raycast(Vector3 origin, Vector3 direction, float maxDist,
                    int& outX, int& outY, int& outZ,
                    int& prevX, int& prevY, int& prevZ) const {
    Vector3 dir = Vector3Normalize(direction);
    float t = 0.0f;
    const float step = 0.05f;

    int lastX = (int)std::floor(origin.x);
    int lastY = (int)std::floor(origin.y);
    int lastZ = (int)std::floor(origin.z);

    while (t < maxDist) {
        Vector3 p = Vector3Add(origin, Vector3Scale(dir, t));
        int bx = (int)std::floor(p.x);
        int by = (int)std::floor(p.y);
        int bz = (int)std::floor(p.z);

        if (bx != lastX || by != lastY || bz != lastZ) {
            Block b = GetBlock(bx, by, bz);
            if (b.IsSolid() || b.type == BlockType::Leaves) {
                outX = bx; outY = by; outZ = bz;
                prevX = lastX; prevY = lastY; prevZ = lastZ;
                return true;
            }
            lastX = bx; lastY = by; lastZ = bz;
        }
        t += step;
    }
    return false;
}