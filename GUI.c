#include "raylib.h"
#include "Graph.h"
#include <math.h>
#include <stdio.h>

#define MAX_NODES 15

typedef struct
{
    Vector2 pos[MAX_NODES];
} Layout;

void computeLayout(Layout *layout, int N, Vector2 center)
{
    float radius = 220.0f;
    float angleStep = 2 * PI / N;

    for (int i = 0; i < N; i++)
    {
        float angle = i * angleStep;

        layout->pos[i].x = center.x + radius * cosf(angle);
        layout->pos[i].y = center.y + radius * sinf(angle);
    }
}

void drawArrow(Vector2 a, Vector2 b)
{
    DrawLineV(a, b, BLACK);

    // arrow head
    Vector2 dir = {b.x - a.x, b.y - a.y};
    float len = sqrtf(dir.x * dir.x + dir.y * dir.y);
    if (len == 0)
        return;

    dir.x /= len;
    dir.y /= len;

    Vector2 left = {
        b.x - dir.x * 15 - dir.y * 7,
        b.y - dir.y * 15 + dir.x * 7};

    Vector2 right = {
        b.x - dir.x * 15 + dir.y * 7,
        b.y - dir.y * 15 - dir.x * 7};

    DrawLineV(b, left, BLACK);
    DrawLineV(b, right, BLACK);
}

void drawGraph(Graph *g, Layout *layout)
{
    int N = g->N;

    // draw edges first
    for (int u = 0; u < N; u++)
    {
        for (int v = 0; v < N; v++)
        {
            if (g->matrix[u][v] != INF && u != v)
            {
                drawArrow(layout->pos[u], layout->pos[v]);

                Vector2 mid = {
                    (layout->pos[u].x + layout->pos[v].x) / 2,
                    (layout->pos[u].y + layout->pos[v].y) / 2};

                DrawText(TextFormat("%d", g->matrix[u][v]),
                         mid.x, mid.y,
                         15, RED);
            }
        }
    }

    // draw nodes on top
    for (int i = 0; i < N; i++)
    {
        DrawCircle(layout->pos[i].x, layout->pos[i].y, 20, SKYBLUE);
        DrawCircleLines(layout->pos[i].x, layout->pos[i].y, 20, DARKBLUE);

        DrawText(TextFormat("%d", i),
                 layout->pos[i].x - 5,
                 layout->pos[i].y - 10,
                 20,
                 BLACK);
    }
}

int main()
{
    // load graph (from your parser)
    Graph *g = parseGraph("input");

    InitWindow(900, 700, "Graph Visualization");

    SetTargetFPS(60);

    Layout layout;
    computeLayout(&layout, g->N, (Vector2){450, 350});

    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        DrawText("Graph Visualization (Adjacency Matrix)", 10, 10, 20, DARKGRAY);

        drawGraph(g, &layout);

        EndDrawing();
    }

    CloseWindow();

    freeGraph(g);

    return 0;
}