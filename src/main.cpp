#include "raylib.h"
#include "raymath.h"
#include "World.h"
#include "Player.h"
#include <cstdio>
#include <string>

enum class GameState {
    MainMenu,
    Playing,
    Paused
};

int main() {
    const int screenW = 1280;
    const int screenH = 720;

    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(screenW, screenH, "VoxelWorld - Minecraft-like");
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);
    DisableCursor();

    GameState state = GameState::MainMenu;
    World* world = nullptr;
    Player player;

    BlockType selectedBlock = BlockType::Grass;
    int selectedIndex = 1;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        // ---------- Input / Update ----------
        if (state == GameState::MainMenu) {
            EnableCursor();
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                delete world;
                world = new World(static_cast<unsigned int>(GetTime() * 1000.0));
                player = Player{};
                player.position = { 8.0f, 50.0f, 8.0f };
                state = GameState::Playing;
                DisableCursor();
            }
            if (IsKeyPressed(KEY_ESCAPE)) break;
        }
        else if (state == GameState::Playing) {
            if (IsKeyPressed(KEY_ESCAPE)) {
                state = GameState::Paused;
                EnableCursor();
            }

            player.Update(*world, dt);
            world->Update(player.position);

            // Mining / placing
            Camera3D cam = player.GetCamera();
            Vector3 forward = Vector3Subtract(cam.target, cam.position);

            int hx, hy, hz, px, py, pz;
            bool hit = world->Raycast(cam.position, forward, 6.0f, hx, hy, hz, px, py, pz);

            if (hit) {
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    if (world->GetBlock(hx, hy, hz).type != BlockType::Bedrock) {
                        world->SetBlock(hx, hy, hz, BlockType::Air);
                    }
                }
                if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
                    world->SetBlock(px, py, pz, selectedBlock);
                }
            }

            // Hotbar
            if (IsKeyPressed(KEY_ONE))   { selectedBlock = BlockType::Grass;   selectedIndex = 1; }
            if (IsKeyPressed(KEY_TWO))   { selectedBlock = BlockType::Dirt;    selectedIndex = 2; }
            if (IsKeyPressed(KEY_THREE)) { selectedBlock = BlockType::Stone;   selectedIndex = 3; }
            if (IsKeyPressed(KEY_FOUR))  { selectedBlock = BlockType::Sand;    selectedIndex = 4; }
            if (IsKeyPressed(KEY_FIVE))  { selectedBlock = BlockType::Wood;    selectedIndex = 5; }
            if (IsKeyPressed(KEY_SIX))   { selectedBlock = BlockType::Leaves;  selectedIndex = 6; }
            if (IsKeyPressed(KEY_SEVEN)) { selectedBlock = BlockType::Water;   selectedIndex = 7; }
            if (IsKeyPressed(KEY_EIGHT)) { selectedBlock = BlockType::Bedrock; selectedIndex = 8; }

            float wheel = GetMouseWheelMove();
            if (wheel != 0) {
                selectedIndex += (wheel > 0 ? -1 : 1);
                if (selectedIndex < 1) selectedIndex = 8;
                if (selectedIndex > 8) selectedIndex = 1;
                selectedBlock = static_cast<BlockType>(selectedIndex);
            }
        }
        else if (state == GameState::Paused) {
            if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER)) {
                state = GameState::Playing;
                DisableCursor();
            }
            if (IsKeyPressed(KEY_Q)) {
                delete world;
                world = nullptr;
                state = GameState::MainMenu;
            }
        }

        // ---------- Render ----------
        BeginDrawing();
        ClearBackground(Color{ 135, 206, 235, 255 });

        if (state == GameState::MainMenu) {
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{ 30, 40, 50, 255 });

            const char* title = "VOXEL WORLD";
            int tw = MeasureText(title, 60);
            DrawText(title, GetScreenWidth()/2 - tw/2, GetScreenHeight()/2 - 120, 60, Color{ 90, 200, 90, 255 });

            const char* subtitle = "A Minecraft-like in C++";
            int sw = MeasureText(subtitle, 24);
            DrawText(subtitle, GetScreenWidth()/2 - sw/2, GetScreenHeight()/2 - 50, 24, LIGHTGRAY);

            const char* prompt = "Press ENTER or CLICK to Play";
            int pw = MeasureText(prompt, 28);
            DrawText(prompt, GetScreenWidth()/2 - pw/2, GetScreenHeight()/2 + 40, 28, RAYWHITE);

            DrawText("ESC to Quit", 20, GetScreenHeight() - 40, 20, GRAY);
        }
        else {
            Camera3D cam = player.GetCamera();
            BeginMode3D(cam);
            world->Draw();
            EndMode3D();

            // Underwater blue tint
            if (player.headInWater) {
                DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                              Color{ 0, 40, 120, 140 });
            }

            // Crosshair
            int cx = GetScreenWidth() / 2;
            int cy = GetScreenHeight() / 2;
            DrawRectangle(cx - 8, cy - 1, 16, 2, WHITE);
            DrawRectangle(cx - 1, cy - 8, 2, 16, WHITE);

            // Hotbar
            const int slotSize = 48;
            const int slots = 8;
            int barW = slots * (slotSize + 8);
            int barX = GetScreenWidth()/2 - barW/2;
            int barY = GetScreenHeight() - 70;

            DrawRectangle(barX - 8, barY - 8, barW + 8, slotSize + 16, Color{ 0, 0, 0, 150 });

            BlockType hotbar[] = {
                BlockType::Grass, BlockType::Dirt, BlockType::Stone,
                BlockType::Sand,  BlockType::Wood, BlockType::Leaves,
                BlockType::Water, BlockType::Bedrock
            };

            for (int i = 0; i < slots; ++i) {
                int sx = barX + i * (slotSize + 8);
                Color col = GetBlockColor(hotbar[i], 0);
                DrawRectangle(sx, barY, slotSize, slotSize, col);
                DrawRectangleLines(sx, barY, slotSize, slotSize,
                    (i + 1 == selectedIndex) ? YELLOW : DARKGRAY);
                DrawText(TextFormat("%d", i + 1), sx + 4, barY + 4, 16, BLACK);
            }

            // HUD
            DrawFPS(10, 10);
            DrawText(TextFormat("Pos: %.1f  %.1f  %.1f", player.position.x, player.position.y, player.position.z), 10, 40, 20, WHITE);
            DrawText(TextFormat("Selected: %s", GetBlockName(selectedBlock)), 10, 70, 20, WHITE);
            if (player.flying) DrawText("FLYING (F to toggle)", 10, 100, 20, YELLOW);
            if (player.inWater) DrawText("Swimming", 10, 130, 20, Color{ 100, 180, 255, 255 });

            DrawText("WASD move | Space jump/swim | Ctrl sink | Shift sprint | F fly | LMB break | RMB place | 1-8 select", 
                     10, GetScreenHeight() - 30, 18, Color{ 200, 200, 200, 200 });

            if (state == GameState::Paused) {
                DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{ 0, 0, 0, 160 });
                const char* paused = "PAUSED";
                int pw = MeasureText(paused, 50);
                DrawText(paused, GetScreenWidth()/2 - pw/2, GetScreenHeight()/2 - 60, 50, WHITE);
                DrawText("ENTER / ESC - Resume", GetScreenWidth()/2 - 120, GetScreenHeight()/2 + 10, 24, LIGHTGRAY);
                DrawText("Q - Main Menu", GetScreenWidth()/2 - 80, GetScreenHeight()/2 + 50, 24, LIGHTGRAY);
            }
        }

        EndDrawing();
    }

    delete world;
    CloseWindow();
    return 0;
}