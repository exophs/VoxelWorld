#include "Chunk.h"
#include "World.h"
#include <vector>
#include <cstring>
#include <algorithm>

Block Chunk::Get(int x, int y, int z) const {
    if (!InBounds(x, y, z)) return Block{ BlockType::Air };
    return blocks[x][y][z];
}

void Chunk::Set(int x, int y, int z, BlockType type) {
    if (!InBounds(x, y, z)) return;
    blocks[x][y][z].type = type;
    dirty = true;
}

bool Chunk::InBounds(int x, int y, int z) const {
    return x >= 0 && x < CHUNK_SIZE_X &&
           y >= 0 && y < CHUNK_SIZE_Y &&
           z >= 0 && z < CHUNK_SIZE_Z;
}

void Chunk::Unload() {
    if (hasMesh) {
        UnloadModel(model);
        hasMesh = false;
    }
    if (hasWaterMesh) {
        UnloadModel(waterModel);
        hasWaterMesh = false;
    }
}

static float FaceShade(int face) {
    switch (face) {
        case 0: return 1.00f;
        case 1: return 0.80f;
        case 2: return 0.60f;
        default: return 1.0f;
    }
}

static int VertexAO(bool side1, bool side2, bool corner) {
    if (side1 && side2) return 0;
    return 3 - ((side1 ? 1 : 0) + (side2 ? 1 : 0) + (corner ? 1 : 0));
}

static float AOMultiplier(int ao) {
    static const float table[4] = { 0.55f, 0.70f, 0.85f, 1.00f };
    return table[std::clamp(ao, 0, 3)];
}

static Color ApplyShade(Color base, float shade) {
    return Color{
        (unsigned char)std::clamp((int)(base.r * shade), 0, 255),
        (unsigned char)std::clamp((int)(base.g * shade), 0, 255),
        (unsigned char)std::clamp((int)(base.b * shade), 0, 255),
        base.a
    };
}

namespace {
    struct MeshBuffers {
        std::vector<Vector3> vertices;
        std::vector<Vector3> normals;
        std::vector<Color>   colors;
        std::vector<unsigned short> indices;
    };

    void AddQuad(MeshBuffers& buf,
                 Vector3 v0, Vector3 v1, Vector3 v2, Vector3 v3,
                 Vector3 normal,
                 Color c0, Color c1, Color c2, Color c3) {
        unsigned short base = static_cast<unsigned short>(buf.vertices.size());
        buf.vertices.push_back(v0);
        buf.vertices.push_back(v1);
        buf.vertices.push_back(v2);
        buf.vertices.push_back(v3);

        for (int i = 0; i < 4; ++i) buf.normals.push_back(normal);

        buf.colors.push_back(c0);
        buf.colors.push_back(c1);
        buf.colors.push_back(c2);
        buf.colors.push_back(c3);

        buf.indices.push_back(base + 0);
        buf.indices.push_back(base + 1);
        buf.indices.push_back(base + 2);
        buf.indices.push_back(base + 0);
        buf.indices.push_back(base + 2);
        buf.indices.push_back(base + 3);
    }

    bool UploadBuffers(MeshBuffers& buf, Mesh& outMesh, Model& outModel) {
        if (buf.vertices.empty()) return false;

        outMesh = { 0 };
        outMesh.vertexCount   = (int)buf.vertices.size();
        outMesh.triangleCount = (int)(buf.indices.size() / 3);

        outMesh.vertices = (float*)MemAlloc(outMesh.vertexCount * 3 * sizeof(float));
        outMesh.normals  = (float*)MemAlloc(outMesh.vertexCount * 3 * sizeof(float));
        outMesh.colors   = (unsigned char*)MemAlloc(outMesh.vertexCount * 4 * sizeof(unsigned char));
        outMesh.indices  = (unsigned short*)MemAlloc(buf.indices.size() * sizeof(unsigned short));

        for (int i = 0; i < outMesh.vertexCount; ++i) {
            outMesh.vertices[i*3+0] = buf.vertices[i].x;
            outMesh.vertices[i*3+1] = buf.vertices[i].y;
            outMesh.vertices[i*3+2] = buf.vertices[i].z;
            outMesh.normals[i*3+0]  = buf.normals[i].x;
            outMesh.normals[i*3+1]  = buf.normals[i].y;
            outMesh.normals[i*3+2]  = buf.normals[i].z;
            outMesh.colors[i*4+0]   = buf.colors[i].r;
            outMesh.colors[i*4+1]   = buf.colors[i].g;
            outMesh.colors[i*4+2]   = buf.colors[i].b;
            outMesh.colors[i*4+3]   = buf.colors[i].a;
        }
        std::memcpy(outMesh.indices, buf.indices.data(), buf.indices.size() * sizeof(unsigned short));

        UploadMesh(&outMesh, false);
        outModel = LoadModelFromMesh(outMesh);
        return true;
    }
}

void Chunk::BuildMesh(const World& world) {
    Unload();

    MeshBuffers opaque;
    MeshBuffers water;

    const int baseX = chunkX * CHUNK_SIZE_X;
    const int baseZ = chunkZ * CHUNK_SIZE_Z;

    auto getType = [&](int nx, int ny, int nz) -> BlockType {
        if (nx >= 0 && nx < CHUNK_SIZE_X &&
            ny >= 0 && ny < CHUNK_SIZE_Y &&
            nz >= 0 && nz < CHUNK_SIZE_Z) {
            return blocks[nx][ny][nz].type;
        }
        return world.GetBlock(baseX + nx, ny, baseZ + nz).type;
    };

    auto isOpaque = [&](int nx, int ny, int nz) -> bool {
        BlockType t = getType(nx, ny, nz);
        return t != BlockType::Air && t != BlockType::Leaves && t != BlockType::Water;
    };

    auto isAir = [&](int nx, int ny, int nz) -> bool {
        return getType(nx, ny, nz) == BlockType::Air;
    };

    auto isWaterAt = [&](int nx, int ny, int nz) -> bool {
        return getType(nx, ny, nz) == BlockType::Water;
    };

    for (int x = 0; x < CHUNK_SIZE_X; ++x) {
        for (int y = 0; y < CHUNK_SIZE_Y; ++y) {
            for (int z = 0; z < CHUNK_SIZE_Z; ++z) {
                BlockType type = blocks[x][y][z].type;
                if (type == BlockType::Air) continue;

                float wx = (float)(baseX + x);
                float wy = (float)y;
                float wz = (float)(baseZ + z);

                const bool isWater = (type == BlockType::Water);
                MeshBuffers& target = isWater ? water : opaque;
                const float topY = isWater ? (wy + 0.9f) : (wy + 1.0f);

                bool drawTop = isWater ? (getType(x, y + 1, z) != BlockType::Water && isAir(x, y + 1, z)) : !isOpaque(x, y + 1, z);
                if (drawTop) {
                    Color base = GetBlockColor(type, 0);
                    float face = FaceShade(0);

                    bool s1 = isOpaque(x - 1, y + 1, z);
                    bool s2 = isOpaque(x,     y + 1, z - 1);
                    bool s3 = isOpaque(x + 1, y + 1, z);
                    bool s4 = isOpaque(x,     y + 1, z + 1);
                    bool c1 = isOpaque(x - 1, y + 1, z - 1);
                    bool c2 = isOpaque(x + 1, y + 1, z - 1);
                    bool c3 = isOpaque(x + 1, y + 1, z + 1);
                    bool c4 = isOpaque(x - 1, y + 1, z + 1);

                    Color col0 = ApplyShade(base, face * AOMultiplier(VertexAO(s1, s2, c1)));
                    Color col1 = ApplyShade(base, face * AOMultiplier(VertexAO(s1, s4, c4)));
                    Color col2 = ApplyShade(base, face * AOMultiplier(VertexAO(s3, s4, c3)));
                    Color col3 = ApplyShade(base, face * AOMultiplier(VertexAO(s3, s2, c2)));

                    AddQuad(target,
                        { wx,     topY, wz     },
                        { wx,     topY, wz + 1 },
                        { wx + 1, topY, wz + 1 },
                        { wx + 1, topY, wz     },
                        { 0, 1, 0 },
                        col0, col1, col2, col3);
                }

                if (!isWater && !isOpaque(x, y - 1, z)) {
                    Color base = GetBlockColor(type, 2);
                    float face = FaceShade(2);

                    bool s1 = isOpaque(x - 1, y - 1, z);
                    bool s2 = isOpaque(x,     y - 1, z - 1);
                    bool s3 = isOpaque(x + 1, y - 1, z);
                    bool s4 = isOpaque(x,     y - 1, z + 1);
                    bool c1 = isOpaque(x - 1, y - 1, z - 1);
                    bool c2 = isOpaque(x + 1, y - 1, z - 1);
                    bool c3 = isOpaque(x + 1, y - 1, z + 1);
                    bool c4 = isOpaque(x - 1, y - 1, z + 1);

                    Color col0 = ApplyShade(base, face * AOMultiplier(VertexAO(s1, s2, c1)));
                    Color col1 = ApplyShade(base, face * AOMultiplier(VertexAO(s3, s2, c2)));
                    Color col2 = ApplyShade(base, face * AOMultiplier(VertexAO(s3, s4, c3)));
                    Color col3 = ApplyShade(base, face * AOMultiplier(VertexAO(s1, s4, c4)));

                    AddQuad(target,
                        { wx,     wy, wz     },
                        { wx + 1, wy, wz     },
                        { wx + 1, wy, wz + 1 },
                        { wx,     wy, wz + 1 },
                        { 0, -1, 0 },
                        col0, col1, col2, col3);
                }

                auto sideBlocked = [&](int nx, int ny, int nz) -> bool {
                    if (isWater) return isOpaque(nx, ny, nz) || isWaterAt(nx, ny, nz);
                    return isOpaque(nx, ny, nz);
                };

                auto drawSide = [&](int nx, int ny, int nz,
                    Vector3 v0, Vector3 v1, Vector3 v2, Vector3 v3,
                    Vector3 normal) {
                        if (sideBlocked(nx, ny, nz)) return;

                        Color base = GetBlockColor(type, 1);
                        float face = FaceShade(1);
                        Color col = ApplyShade(base, face * 0.9f);
                        AddQuad(target, v0, v1, v2, v3, normal, col, col, col, col);
                 };

                drawSide(x + 1, y, z,
                    { wx + 1, wy,   wz     },
                    { wx + 1, topY, wz     },
                    { wx + 1, topY, wz + 1 },
                    { wx + 1, wy,   wz + 1 },
                    { 1, 0, 0 });

                drawSide(x - 1, y, z,
                    { wx, wy,   wz + 1 },
                    { wx, topY, wz + 1 },
                    { wx, topY, wz     },
                    { wx, wy,   wz     },
                    { -1, 0, 0 });

                drawSide(x, y, z + 1,
                    { wx,     wy,   wz + 1 },
                    { wx + 1, wy,   wz + 1 },
                    { wx + 1, topY, wz + 1 },
                    { wx,     topY, wz + 1 },
                    { 0, 0, 1 });

                drawSide(x, y, z - 1,
                    { wx + 1, wy,   wz },
                    { wx,     wy,   wz },
                    { wx,     topY, wz },
                    { wx + 1, topY, wz },
                    { 0, 0, -1 });
            }
        }
    }

    hasMesh = UploadBuffers(opaque, mesh, model);
    hasWaterMesh = UploadBuffers(water, waterMesh, waterModel);
    dirty = false;
}