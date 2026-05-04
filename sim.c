#include "raylib.h"
#include "Graph.h"
#include "GUI.h"
#include <stdio.h>

int main(int argc, char **argv)
{
    // -------------------------
    // Check input file
    // -------------------------
    if (argc != 2)
    {
        printf("Usage: %s <input_file>\n", argv[0]);
        return 1;
    }

    // -------------------------
    // Load graph
    // -------------------------
    Graph *g = parseGraph(argv[1]);
    if (!g)
    {
        printf("Failed to load graph\n");
        return 1;
    }

    // -------------------------
    // Init window
    // -------------------------
    InitWindow(900, 700, "Graph Visualization - Milestone 2");
    SetTargetFPS(60);

    // -------------------------
    // Compute layout once
    // -------------------------
    Layout layout;
    computeLayout(&layout, g->N, (Vector2){450, 350});

    // -------------------------
    // Play / Stop state
    // -------------------------
    bool isPlaying = false;

    // Button settings
    Rectangle button = {700, 20, 150, 50};

    // -------------------------
    // Main render loop
    // -------------------------
    while (!WindowShouldClose())
    {
        // Mouse
        Vector2 mouse = GetMousePosition();
        bool hover = CheckCollisionPointRec(mouse, button);

        // Click
        if (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            isPlaying = !isPlaying;
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);

        // Title
        DrawText("Graph Visualization (Milestone 2)", 10, 10, 20, DARKGRAY);

        // -------------------------
        // Draw Graph
        // -------------------------
        drawGraph(g, &layout);

        // -------------------------
        // Draw Button
        // -------------------------
        Color btnColor;

        if (hover)
            btnColor = LIGHTGRAY;
        else
            btnColor = GRAY;

        DrawRectangleRec(button, btnColor);
        DrawRectangleLinesEx(button, 2, BLACK);

        const char *text = isPlaying ? "Stop" : "Play";

        int textWidth = MeasureText(text, 20);

        DrawText(text,
                 button.x + button.width / 2 - textWidth / 2,
                 button.y + 15,
                 20,
                 BLACK);

        // -------------------------
        // Optional: show state
        // -------------------------
        DrawText(isPlaying ? "Running" : "Paused", 20, 50, 20, RED);

        EndDrawing();
    }

    // -------------------------
    // Cleanup
    // -------------------------
    CloseWindow();
    freeGraph(g);

    return 0;
}
