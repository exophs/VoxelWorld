#pragma once
#include "Chunk.h"
#include "Noise.h"
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>

struct ChunkCoord {
    int x, z;
    bool operator==(const ChunkCoord& o) const { return x == o.x && z == o.z; }
};

struct ChunkCoordHash {
    size_t operator()(const ChunkCoord& c) const {
        return std::hash<int>()(c.x) ^ (std::hash<int>()(c.z) << 1);
    }
};

struct LodChunk {
    int chunkX = 0;
    int chunkZ = 0;
    int lodLevel = 1;

    Mesh mesh{};
    Model model{};
    bool hasMesh = false;

    Mesh waterMesh{};
    Model waterModel{};
    bool hasWaterMesh = false;

    bool generated = false;

    void Unload() {
        if (hasMesh) {
            UnloadModel(model);
            hasMesh = false;
        }
        if (hasWaterMesh) {
            UnloadModel(waterModel);
            hasWaterMesh = false;
        }
    }
};

class World {
public:
    explicit World(unsigned int seed = 42);
    ~World();

    void Update(const Vector3& playerPos);
    void Draw() const;

    Block GetBlock(int wx, int wy, int wz) const;
    void SetBlock(int wx, int wy, int wz, BlockType type);

    bool Raycast(Vector3 origin, Vector3 direction, float maxDist,
                 int& outX, int& outY, int& outZ,
                 int& prevX, int& prevY, int& prevZ) const;

    int GetSeed() const { return m_seed; }

private:
    unsigned int m_seed;
    Noise m_noise;

    std::unordered_map<ChunkCoord, std::unique_ptr<Chunk>, ChunkCoordHash> m_chunks;
    std::unordered_map<ChunkCoord, std::unique_ptr<LodChunk>, ChunkCoordHash> m_lodChunks;

    static constexpr int LOD0_DISTANCE = 5;
    static constexpr int LOD1_DISTANCE = 10;
    static constexpr int LOD2_DISTANCE = 18;

    Chunk* GetChunk(int cx, int cz);
    const Chunk* GetChunk(int cx, int cz) const;
    void EnsureChunk(int cx, int cz);
    void GenerateChunk(Chunk& chunk);
    void PlaceTree(Chunk& chunk, int lx, int surfaceY, int lz);

    void EnsureLodChunk(int cx, int cz, int lodLevel);
    void GenerateLodMesh(LodChunk& lod);
};