#include "raylib.h"
#include "Graph.h"
#include "GUI.h"
#include <stdio.h>
#include <stdbool.h>
#include <math.h>

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
    InitWindow(900, 700, "Graph Visualization - Milestone 3");
    SetTargetFPS(60);

    // -------------------------
    // Compute layout once (static graph)
    // -------------------------
    Layout layout;
    computeLayout(&layout, g->N, (Vector2){450, 350});

    // -------------------------
    // Build shortest path once
    // -------------------------
    int pathSize = 0;
    int *path = dijkstra(g, &pathSize);

    bool hasPath = (path != NULL);
    bool isAnimating = false;

    Rectangle playStopBtn = {20.0f, 50.0f, 120.0f, 40.0f};
    int currentNodeIndex = 0;
    float moveSpeed = 120.0f;
    Vector2 agentPos = {0};

    if (hasPath)
    {
        agentPos = layout.pos[path[0]];
    }

    // -------------------------
    // Main render loop
    // -------------------------
    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();

        bool clickedButton = CheckCollisionPointRec(GetMousePosition(), playStopBtn) &&
                             IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

        // Toggle animation state (Play/Stop)
        if ((clickedButton || IsKeyPressed(KEY_SPACE)) && hasPath)
        {
            isAnimating = !isAnimating;
        }

        // Run movement logic only while animation is active
        if (hasPath && isAnimating && currentNodeIndex < pathSize - 1)
        {
            Vector2 target = layout.pos[path[currentNodeIndex + 1]];
            Vector2 delta = {target.x - agentPos.x, target.y - agentPos.y};
            float distance = sqrtf(delta.x * delta.x + delta.y * delta.y);
            float step = moveSpeed * dt;

            if (distance <= step)
            {
                agentPos = target;
                currentNodeIndex++;
            }
            else
            {
                agentPos.x += (delta.x / distance) * step;
                agentPos.y += (delta.y / distance) * step;
            }
        }

        if (hasPath && currentNodeIndex >= pathSize - 1)
        {
            isAnimating = false;
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);

        // Title
        DrawText("Graph Visualization (Milestone 3)", 10, 10, 20, DARKGRAY);

        // Draw graph
        drawGraph(g, &layout);

        // Draw play/stop button
        DrawRectangleRec(playStopBtn, LIGHTGRAY);
        DrawRectangleLines((int)playStopBtn.x, (int)playStopBtn.y, (int)playStopBtn.width, (int)playStopBtn.height, DARKGRAY);
        DrawText(isAnimating ? "Stop" : "Play",
                 (int)playStopBtn.x + 35,
                 (int)playStopBtn.y + 11,
                 20,
                 BLACK);

        if (!hasPath)
        {
            DrawText("No path found between source and destination.", 20, 105, 20, RED);
        }
        else
        {
            // Animated entity
            DrawCircle((int)agentPos.x, (int)agentPos.y, 10, ORANGE);
            DrawCircleLines((int)agentPos.x, (int)agentPos.y, 10, DARKBROWN);

            if (currentNodeIndex >= pathSize - 1)
            {
                DrawText("Arrived to destination!", 20, 105, 24, GREEN);
            }
        }

        EndDrawing();
    }

    // -------------------------
    // Cleanup
    // -------------------------
    CloseWindow();
    free(path);
    freeGraph(g);

    return 0;
}