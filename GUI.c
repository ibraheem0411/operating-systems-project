#include "GUI.h"
#include <math.h>
#include <stdio.h>

#ifndef PI
#define PI 3.14159265358979323846f
#endif

// -------------------------
// Layout (circle placement)
// -------------------------
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

// -------------------------
// Draw arrow helper
// -------------------------
static void drawArrow(Vector2 a, Vector2 b)
{
    DrawLineV(a, b, BLACK);

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

// -------------------------
// Draw full graph
// -------------------------
void drawGraph(Graph *g, Layout *layout)
{
    int N = g->N;

    // -------- edges first --------
    for (int u = 0; u < N; u++)
    {
        for (int v = 0; v < N; v++)
        {
            if (g->matrix[u][v] != INF && u != v)
            {
                Vector2 a = layout->pos[u];
                Vector2 b = layout->pos[v];

                drawArrow(a, b);

                // weight label in middle
                Vector2 mid = {
                    (a.x + b.x) / 2.0f,
                    (a.y + b.y) / 2.0f};

                DrawText(TextFormat("%d", g->matrix[u][v]),
                         (int)mid.x,
                         (int)mid.y,
                         15,
                         RED);
            }
        }
    }

    // -------- nodes on top --------
    for (int i = 0; i < N; i++)
    {
        Vector2 p = layout->pos[i];

        DrawCircle((int)p.x, (int)p.y, 20, SKYBLUE);
        DrawCircleLines((int)p.x, (int)p.y, 20, DARKBLUE);

        DrawText(TextFormat("%d", i),
                 (int)p.x - 5,
                 (int)p.y - 10,
                 20,
                 BLACK);
    }
}