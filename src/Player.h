#pragma once
#include "raylib.h"
#include "World.h"

class Player {
public:
    Vector3 position{ 0, 40, 0 };
    Vector3 velocity{ 0, 0, 0 };
    float yaw = 0.0f;
    float pitch = 0.0f;

    float eyeHeight = 1.6f;
    float width = 0.6f;
    float height = 1.8f;

    bool onGround = false;
    bool flying = false;
    bool inWater = false;
    bool headInWater = false;

    void Update(World& world, float dt);
    Camera3D GetCamera() const;

private:
    void ApplyGravityAndCollisions(World& world, float dt);
    bool IsInWaterAt(const World& world, float yOffset) const;
};