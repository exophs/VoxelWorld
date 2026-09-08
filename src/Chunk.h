#pragma once
#include "Block.h"
#include "raylib.h"
#include <vector>
#include <cstdint>

static constexpr int CHUNK_SIZE_X = 16;
static constexpr int CHUNK_SIZE_Y = 64;
static constexpr int CHUNK_SIZE_Z = 16;

class Chunk {
public:
    int chunkX = 0;
    int chunkZ = 0;
    bool dirty = true;
    bool generated = false;
    Block blocks[CHUNK_SIZE_X][CHUNK_SIZE_Y][CHUNK_SIZE_Z]{};

    Mesh mesh{};
    Model model{};
    bool hasMesh = false;

    Mesh waterMesh{};
    Model waterModel{};
    bool hasWaterMesh = false;

    Block Get(int x, int y, int z) const;
    void Set(int x, int y, int z, BlockType type);
    bool InBounds(int x, int y, int z) const;
    void BuildMesh(const class World& world);
    void Unload();
};