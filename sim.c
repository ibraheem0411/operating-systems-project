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
    // Compute layout once (static graph)
    // -------------------------
    Layout layout;
    computeLayout(&layout, g->N, (Vector2){450, 350});

    // -------------------------
    // Main render loop
    // -------------------------
    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        // Title
        DrawText("Graph Visualization (Milestone 2)", 10, 10, 20, DARKGRAY);

        // Draw graph
        drawGraph(g, &layout);

        EndDrawing();
    }

    // -------------------------
    // Cleanup
    // -------------------------
    CloseWindow();
    freeGraph(g);

    return 0;
}