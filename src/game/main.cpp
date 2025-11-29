#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <raylib.h>
#include <yaml-cpp/yaml.h>
#include <iostream>
#include <string>
#include <fstream>

// Minimal example demonstrating:
// - SDL3_mixer for audio (WAV/OGG)
// - raylib for rendering (high-performance, easy-to-use)
// - yaml-cpp for configuration files

int main(int argc, char* argv[])
{
    // Initialize SDL for audio
    if (SDL_Init(SDL_INIT_AUDIO) != 0) {
        std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
        return 1;
    }

    // Example: YAML configuration loading
    // yaml-cpp usage example:
    // YAML::Node config = YAML::LoadFile("config.yaml");
    // std::string title = config["window"]["title"].as<std::string>();
    // int width = config["window"]["width"].as<int>();

    // Initialize raylib window
    const int screenWidth = 800;
    const int screenHeight = 450;
    
    InitWindow(screenWidth, screenHeight, "AirStrike3D Tools - raylib + SDL3_mixer + yaml-cpp");
    SetTargetFPS(60);

    std::cout << "Libraries initialized successfully!" << std::endl;
    std::cout << "  - raylib: High-performance rendering library" << std::endl;
    std::cout << "  - SDL3_mixer: Audio support (WAV/OGG)" << std::endl;
    std::cout << "  - yaml-cpp: YAML configuration parsing" << std::endl;

    // Main game loop
    while (!WindowShouldClose()) {
        BeginDrawing();
        
        ClearBackground(RAYWHITE);
        
        // Example raylib drawing
        DrawText("raylib + SDL3_mixer + yaml-cpp", 190, 200, 20, LIGHTGRAY);
        DrawText("Press ESC to exit", 300, 230, 15, GRAY);
        
        // Example: Draw a simple shape
        DrawCircle(screenWidth / 2, screenHeight / 2, 50, BLUE);
        
        EndDrawing();
    }

    // Cleanup
    CloseWindow();
    SDL_Quit();

    return 0;
}

