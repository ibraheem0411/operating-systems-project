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
    float timer = 0.0f;
    bool waitingAtNode = false;
    Vector2 agentPos = {0};
    int currentJump=0;
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
        if (isAnimating && hasPath && currentNodeIndex < pathSize - 1){
            timer += dt;
            if (waitingAtNode){
                if (timer >= 1.0f){ // Wait 1 second at each node
                    waitingAtNode = false;
                    timer = 0.0f;
                }
            }
            else {
                int from = path[currentNodeIndex];
                int to = path[currentNodeIndex + 1];
                int weight = g->matrix[from][to];
                if (timer >= 0.3f){ 
                    currentJump++;
                    timer = 0.0f;
                }
                if (currentJump >= weight){
                    currentNodeIndex++;
                    currentJump = 0;
                    agentPos = layout.pos[path[currentNodeIndex]];
                    if (currentNodeIndex < pathSize - 1){
                        waitingAtNode = true;
                    }
                }
                else {
                    float t = (float)currentJump / weight;
                    Vector2 fromPos = layout.pos[from];
                    Vector2 toPos = layout.pos[to];
                    agentPos.x = fromPos.x + t * (toPos.x - fromPos.x);
                    agentPos.y = fromPos.y + t * (toPos.y - fromPos.y);
                }
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