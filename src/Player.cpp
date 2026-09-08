#include "Player.h"
#include "raymath.h"
#include <cmath>
#include <algorithm>

static bool AABBSolid(const World& world, float minX, float minY, float minZ,
                      float maxX, float maxY, float maxZ) {
    int x0 = (int)std::floor(minX);
    int y0 = (int)std::floor(minY);
    int z0 = (int)std::floor(minZ);
    int x1 = (int)std::floor(maxX);
    int y1 = (int)std::floor(maxY);
    int z1 = (int)std::floor(maxZ);

    for (int x = x0; x <= x1; ++x)
        for (int y = y0; y <= y1; ++y)
            for (int z = z0; z <= z1; ++z)
                if (world.GetBlock(x, y, z).IsSolid())
                    return true;
    return false;
}

bool Player::IsInWaterAt(const World& world, float yOffset) const {
    int bx = (int)std::floor(position.x);
    int by = (int)std::floor(position.y + yOffset);
    int bz = (int)std::floor(position.z);
    return world.GetBlock(bx, by, bz).type == BlockType::Water;
}

void Player::Update(World& world, float dt) {
    // Mouse look
    Vector2 mouseDelta = GetMouseDelta();
    const float sensitivity = 0.12f;
    yaw += mouseDelta.x * sensitivity;
    pitch -= mouseDelta.y * sensitivity;
    pitch = std::clamp(pitch, -89.0f, 89.0f);

    // Water state
    inWater     = IsInWaterAt(world, 0.4f);
    headInWater = IsInWaterAt(world, eyeHeight - 0.15f);

    Vector3 forwardH = { std::cos(yaw * DEG2RAD), 0.0f, std::sin(yaw * DEG2RAD) };
    Vector3 right    = { -forwardH.z, 0.0f, forwardH.x };

    float speed = 4.5f;
    if (IsKeyDown(KEY_LEFT_SHIFT)) speed = 7.0f;

    if (flying) {
        speed = IsKeyDown(KEY_LEFT_SHIFT) ? 20.0f : 10.0f;
    }
    else if (inWater) {
        speed = 3.0f;   // slower in water
    }

    Vector3 wish{ 0, 0, 0 };
    if (IsKeyDown(KEY_W)) wish = Vector3Add(wish, forwardH);
    if (IsKeyDown(KEY_S)) wish = Vector3Subtract(wish, forwardH);
    if (IsKeyDown(KEY_A)) wish = Vector3Subtract(wish, right);
    if (IsKeyDown(KEY_D)) wish = Vector3Add(wish, right);

    if (Vector3Length(wish) > 0.001f)
        wish = Vector3Scale(Vector3Normalize(wish), speed);

    if (flying) {
        if (IsKeyDown(KEY_SPACE)) wish.y += speed;
        if (IsKeyDown(KEY_LEFT_CONTROL)) wish.y -= speed;
        velocity = wish;
        position = Vector3Add(position, Vector3Scale(velocity, dt));
        onGround = false;
    }
    else if (inWater) {
        // === Water movement (no auto float) ===
        velocity.x = wish.x;
        velocity.z = wish.z;

        // Only go up when holding Space
        if (IsKeyDown(KEY_SPACE)) {
            velocity.y = 5.0f;          // swim up
        }
        else if (IsKeyDown(KEY_LEFT_CONTROL)) {
            velocity.y = -4.0f;         // sink
        }
        else {
            // Gentle sink when not pressing anything
            velocity.y -= 6.0f * dt;
            if (velocity.y < -2.0f) velocity.y = -2.0f;
            if (velocity.y >  0.0f) velocity.y =  0.0f;
        }

        // Water drag
        velocity.x *= (1.0f - 3.5f * dt);
        velocity.z *= (1.0f - 3.5f * dt);

        ApplyGravityAndCollisions(world, dt);
    }
    else {
        // Normal walking
        velocity.x = wish.x;
        velocity.z = wish.z;

        if (onGround && IsKeyPressed(KEY_SPACE)) {
            velocity.y = 8.5f;
            onGround = false;
        }

        ApplyGravityAndCollisions(world, dt);
    }

    // Toggle fly
    if (IsKeyPressed(KEY_F)) {
        flying = !flying;
        velocity = { 0, 0, 0 };
    }
}

void Player::ApplyGravityAndCollisions(World& world, float dt) {
    if (!inWater && !flying) {
        velocity.y += -28.0f * dt;
        if (velocity.y < -50.0f) velocity.y = -50.0f;
    }

    float half = width * 0.5f;

    // --- X movement ---
    {
        float newX = position.x + velocity.x * dt;
        if (!AABBSolid(world, newX - half, position.y, position.z - half,
                              newX + half, position.y + height, position.z + half)) {
            position.x = newX;
        } else {
            // Small step-up help when trying to get out of water
            if (inWater && velocity.y >= -0.5f) {
                float stepY = position.y + 0.6f;
                if (!AABBSolid(world, newX - half, stepY, position.z - half,
                                      newX + half, stepY + height, position.z + half)) {
                    position.x = newX;
                    position.y = stepY;
                } else {
                    velocity.x = 0;
                }
            } else {
                velocity.x = 0;
            }
        }
    }

    // --- Z movement ---
    {
        float newZ = position.z + velocity.z * dt;
        if (!AABBSolid(world, position.x - half, position.y, newZ - half,
                              position.x + half, position.y + height, newZ + half)) {
            position.z = newZ;
        } else {
            if (inWater && velocity.y >= -0.5f) {
                float stepY = position.y + 0.6f;
                if (!AABBSolid(world, position.x - half, stepY, newZ - half,
                                      position.x + half, stepY + height, newZ + half)) {
                    position.z = newZ;
                    position.y = stepY;
                } else {
                    velocity.z = 0;
                }
            } else {
                velocity.z = 0;
            }
        }
    }

    // --- Y movement ---
    {
        float newY = position.y + velocity.y * dt;
        if (!AABBSolid(world, position.x - half, newY, position.z - half,
                              position.x + half, newY + height, position.z + half)) {
            position.y = newY;
            onGround = false;
        } else {
            if (velocity.y < 0) onGround = true;
            velocity.y = 0;
        }
    }
}

Camera3D Player::GetCamera() const {
    Camera3D cam{};
    cam.position = { position.x, position.y + eyeHeight, position.z };

    Vector3 forward = {
        std::cos(yaw * DEG2RAD) * std::cos(pitch * DEG2RAD),
        std::sin(pitch * DEG2RAD),
        std::sin(yaw * DEG2RAD) * std::cos(pitch * DEG2RAD)
    };
    cam.target = Vector3Add(cam.position, forward);
    cam.up = { 0, 1, 0 };
    cam.fovy = 70.0f;
    cam.projection = CAMERA_PERSPECTIVE;
    return cam;
}