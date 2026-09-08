#pragma once
#include "raylib.h"
#include <cstdint>

enum class BlockType : uint8_t {
    Air = 0,
    Grass,
    Dirt,
    Stone,
    Bedrock,
    Wood,
    Leaves,
    Sand,
    Water,
    Count
};

struct Block {
    BlockType type = BlockType::Air;

    bool IsSolid() const {
        return type != BlockType::Air &&
               type != BlockType::Leaves &&
               type != BlockType::Water;
    }

    bool IsOpaque() const {
        // Water and leaves are transparent for face culling
        return type != BlockType::Air &&
               type != BlockType::Leaves &&
               type != BlockType::Water;
    }
};

Color GetBlockColor(BlockType type, int face); // 0=top, 1=side, 2=bottom
const char* GetBlockName(BlockType type);