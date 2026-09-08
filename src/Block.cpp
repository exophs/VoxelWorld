#include "Block.h"

Color GetBlockColor(BlockType type, int face) {
    switch (type) {
    case BlockType::Grass:
        if (face == 0) return Color{ 90, 170, 60, 255 };   // top
        if (face == 2) return Color{ 120, 85, 55, 255 };    // bottom
        return Color{ 110, 150, 70, 255 };                  // side

    case BlockType::Dirt:
        return Color{ 130, 95, 60, 255 };

    case BlockType::Stone:
        return Color{ 120, 120, 125, 255 };

    case BlockType::Bedrock:
        return Color{ 40, 40, 45, 255 };

    case BlockType::Wood:
        if (face == 0 || face == 2) return Color{ 100, 75, 40, 255 };
        return Color{ 140, 105, 60, 255 };

    case BlockType::Leaves:
        return Color{ 50, 140, 50, 200 };

    case BlockType::Sand:
        return Color{ 220, 210, 150, 255 };

    case BlockType::Water:
        return Color{ 40, 80, 200, 180 };   // semi-transparent blue

    default:
        return Color{ 255, 0, 255, 255 };
    }
}

const char* GetBlockName(BlockType type) {
    switch (type) {
    case BlockType::Air:     return "Air";
    case BlockType::Grass:   return "Grass";
    case BlockType::Dirt:    return "Dirt";
    case BlockType::Stone:   return "Stone";
    case BlockType::Bedrock: return "Bedrock";
    case BlockType::Wood:    return "Wood";
    case BlockType::Leaves:  return "Leaves";
    case BlockType::Sand:    return "Sand";
    case BlockType::Water:   return "Water";
    default:                 return "Unknown";
    }
}